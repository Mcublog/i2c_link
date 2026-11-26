/**
 * @file adapter_test.cpp
 * @brief Сквозные тесты утилиты i2c-link-adapter на паре pty
 * @version 0.1
 * @date 2026-11-26
 *
 * Тест запускает собранную утилиту на slave-стороне псевдотерминала,
 * а мастер-сторона играет роль платы: принимает кадры протокола и
 * отвечает на них.
 *
 */
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <vector>

#include "app/proto/frame.h"
#include "app/proto/proto.h"
#include "cobsr.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME utest
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Счётчик неудачных проверок */
static int m_failures;

/**
 * @brief Проверка условия
 */
#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            LOG_ERROR("check failed: %s:%d: %s", __FILE__, __LINE__, #cond); \
            m_failures++;                                                    \
        }                                                                    \
    } while (0)

/** @brief Таймаут ожидания в тестах, мс */
static const int kTimeoutMs = 2000;

/**
 * @brief Запущенный экземпляр утилиты и её окружение
 */
struct Fixture
{
    pid_t pid{-1}; /**< процесс утилиты */
    int master{-1}; /**< мастер-сторона pty (роль платы) */
    int client{-1}; /**< клиент UNIX-сокета */
    std::string socket_path; /**< путь сокета */
    std::string buffer; /**< накопленные байты с провода */
    std::string client_buffer; /**< накопленные строки клиента */
};
//<<----------------------

//>>---------------------- Private
/**
 * @brief Текущее время, мс
 *
 * @return long long миллисекунды
 */
static long long now_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * @brief Убить и подождать процесс утилиты
 *
 * @param fx окружение
 */
static void stop(Fixture *fx)
{
    if (fx->client >= 0)
    {
        (void)close(fx->client);
        fx->client = -1;
    }
    if (fx->master >= 0)
    {
        (void)close(fx->master);
        fx->master = -1;
    }
    if (fx->pid > 0)
    {
        (void)kill(fx->pid, SIGTERM);
        (void)waitpid(fx->pid, nullptr, 0);
        fx->pid = -1;
    }
    if (!fx->socket_path.empty())
        (void)unlink(fx->socket_path.c_str());
}

/**
 * @brief Подключиться к сокету утилиты
 *
 * @param fx окружение
 * @return true - клиент подключён
 */
static bool connect_client(Fixture *fx)
{
    long long deadline = now_ms() + kTimeoutMs;

    while (now_ms() < deadline)
    {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
            return false;

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s",
                 fx->socket_path.c_str());

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            fx->client = fd;
            return true;
        }
        (void)close(fd);

        if ((fx->pid > 0) && (waitpid(fx->pid, nullptr, WNOHANG) == fx->pid))
        {
            fx->pid = -1;
            return false;
        }

        usleep(5000);
    }

    return false;
}

/**
 * @brief Запустить утилиту на паре pty
 *
 * @param fx окружение
 * @param tag суффикс имени сокета
 * @param args аргументы утилиты (после --listen)
 * @param with_client true - дождаться и подключить клиента
 * @return true - утилита запущена
 */
static bool start(Fixture *fx, const char *tag,
                  std::vector<const char *> args, bool with_client = true)
{
    static const char *kBin = ADAPTER_BIN;

    fx->socket_path =
        std::string("/tmp/i2c-link-adapter-utest-") + tag + ".sock";
    (void)unlink(fx->socket_path.c_str());

    fx->master = posix_openpt(O_RDWR | O_NOCTTY);
    if ((fx->master < 0) || (grantpt(fx->master) != 0) ||
        (unlockpt(fx->master) != 0))
    {
        LOG_ERROR("pty init failed");
        stop(fx);
        return false;
    }

    const char *slave_name = ptsname(fx->master);
    if (slave_name == nullptr)
    {
        LOG_ERROR("ptsname failed");
        stop(fx);
        return false;
    }

    fx->pid = fork();
    if (fx->pid < 0)
    {
        LOG_ERROR("fork failed");
        stop(fx);
        return false;
    }

    if (fx->pid == 0)
    {
        std::vector<const char *> argv;
        argv.push_back("i2c-link-adapter");
        argv.push_back("--tty");
        argv.push_back(slave_name);
        argv.push_back("--listen");
        argv.push_back(fx->socket_path.c_str());
        for (const char *arg : args)
            argv.push_back(arg);
        argv.push_back(nullptr);

        (void)execv(kBin, const_cast<char *const *>(argv.data()));
        _exit(127);
    }

    if (with_client)
    {
        if (!connect_client(fx))
        {
            LOG_ERROR("socket did not appear");
            stop(fx);
            return false;
        }
    }

    return true;
}

/**
 * @brief Закодировать сырой кадр в COBS/R с терминатором
 *
 * @param raw сырой кадр
 * @return std::vector<uint8_t> кадр на проводе
 */
static std::vector<uint8_t> encode(const std::vector<uint8_t> &raw)
{
    uint8_t encoded[PROTO_MAX_COBS_FRAME];
    cobsr_encode_result result =
        cobsr_encode(encoded, sizeof(encoded), raw.data(), raw.size());

    CHECK(result.status == COBSR_ENCODE_OK);

    std::vector<uint8_t> wire(encoded, encoded + result.out_len);
    wire.push_back(0x00);
    return wire;
}

/**
 * @brief Собрать кадр ответного формата
 *
 * @param cmd команда
 * @param msg_id номер без бита направления
 * @param flags флаги
 * @param addr адрес
 * @param status статус
 * @param data payload
 * @param len длина payload
 * @return std::vector<uint8_t> сырой кадр
 */
static std::vector<uint8_t> make_response(uint8_t cmd, uint8_t msg_id,
                                          uint8_t flags, uint8_t addr,
                                          uint8_t status, const uint8_t *data,
                                          uint8_t len)
{
    proto_resp_t resp;
    memset(&resp, 0, sizeof(resp));

    resp.cmd = (proto_cmd_t)cmd;
    resp.msg_id = msg_id;
    resp.flags = flags;
    resp.addr = addr;
    resp.status = (proto_status_t)status;
    resp.len = len;
    resp.data = data;

    std::vector<uint8_t> raw(PROTO_MAX_RAW_FRAME);
    uint32_t size = proto_frame_build_resp(raw.data(),
                                           (uint32_t)raw.size(), &resp);
    CHECK(size != 0);
    raw.resize(size);
    return raw;
}

/**
 * @brief Записать кадр в мастер-сторону pty
 *
 * @param fx окружение
 * @param raw сырой кадр
 */
static void send_to_adapter(Fixture *fx, const std::vector<uint8_t> &raw)
{
    std::vector<uint8_t> wire = encode(raw);
    size_t written = 0;

    while (written < wire.size())
    {
        ssize_t result =
            write(fx->master, wire.data() + written, wire.size() - written);
        if (result <= 0)
        {
            CHECK(false);
            return;
        }
        written += (size_t)result;
    }
}

/**
 * @brief Прочитать кадр с провода (до терминатора)
 *
 * @param fx окружение
 * @param raw раскодированный кадр
 * @param timeout_ms таймаут
 * @return true - кадр получен
 */
static bool read_frame(Fixture *fx, std::vector<uint8_t> *raw,
                       int timeout_ms = kTimeoutMs)
{
    long long deadline = now_ms() + timeout_ms;

    for (;;)
    {
        size_t end = fx->buffer.find('\0');
        if (end != std::string::npos)
        {
            std::string wire = fx->buffer.substr(0, end);
            fx->buffer.erase(0, end + 1);

            uint8_t decoded[PROTO_MAX_RAW_FRAME];
            cobsr_decode_result result =
                cobsr_decode(decoded, sizeof(decoded),
                             (const uint8_t *)wire.data(), wire.size());
            CHECK(result.status == COBSR_DECODE_OK);
            raw->assign(decoded, decoded + result.out_len);
            return true;
        }

        long long left = deadline - now_ms();
        if (left <= 0)
            return false;

        struct pollfd pfd = {fx->master, POLLIN, 0};
        int ready = poll(&pfd, 1, (int)left);
        if (ready < 0)
            return false;
        if (ready == 0)
            continue;
        if ((pfd.revents & (POLLERR | POLLHUP)) != 0)
            return false;

        char chunk[256];
        ssize_t size = read(fx->master, chunk, sizeof(chunk));
        if (size <= 0)
            return false;
        fx->buffer.append(chunk, (size_t)size);
    }
}

/**
 * @brief Прочитать строку от клиента
 *
 * @param fx окружение
 * @param timeout_ms таймаут
 * @return std::string строка без перевода строки (пустая - таймаут)
 */
static std::string read_line(Fixture *fx, int timeout_ms = kTimeoutMs)
{
    long long deadline = now_ms() + timeout_ms;

    for (;;)
    {
        size_t end = fx->client_buffer.find('\n');
        if (end != std::string::npos)
        {
            std::string result = fx->client_buffer.substr(0, end);
            fx->client_buffer.erase(0, end + 1);
            if (!result.empty() && (result.back() == '\r'))
                result.pop_back();
            return result;
        }

        long long left = deadline - now_ms();
        if (left <= 0)
            return std::string();

        struct pollfd pfd = {fx->client, POLLIN, 0};
        int ready = poll(&pfd, 1, (int)left);
        if (ready <= 0)
            continue;

        char chunk[256];
        ssize_t size = recv(fx->client, chunk, sizeof(chunk), 0);
        if (size <= 0)
            return std::string();
        fx->client_buffer.append(chunk, (size_t)size);
    }
}

/**
 * @brief Отправить команду клиента
 *
 * @param fx окружение
 * @param text команда
 */
static void send_line(Fixture *fx, const std::string &text)
{
    std::string line = text + "\r\n";
    ssize_t size = send(fx->client, line.data(), line.size(), 0);
    CHECK(size == (ssize_t)line.size());
}

/**
 * @brief Разобрать кадр-запрос, полученный от утилиты
 *
 * @param raw сырой кадр
 * @param req результат разбора
 * @return true - кадр корректен
 */
static bool parse_request(const std::vector<uint8_t> &raw, proto_req_t *req)
{
    return proto_frame_parse(raw.data(), (uint32_t)raw.size(), req) ==
           PROTO_STATUS_OK;
}

/**
 * @brief Проверка события записи+чтения и ответа клиента
 */
static void test_event_and_reply()
{
    Fixture fx;

    if (!start(&fx, "event",
               {"--semantic", "--no-config", "--reply-timeout", "500"}))
        return;

    static const uint8_t kWritten[] = {0x10, 0x20};
    send_to_adapter(&fx,
                    make_response(PROTO_CMD_SLAVE_EVENT, 5,
                                  PROTO_EVENT_FLAG_WRITE |
                                      PROTO_EVENT_FLAG_READ,
                                  0x42, PROTO_STATUS_OK, kWritten,
                                  sizeof(kWritten)));

    CHECK(read_line(&fx) == "EVENT 5 WR 42 1020");

    send_line(&fx, "REPLY 5 AABB");
    CHECK(read_line(&fx) == "OK");

    std::vector<uint8_t> raw;
    CHECK(read_frame(&fx, &raw));

    proto_req_t req;
    CHECK(parse_request(raw, &req));
    CHECK(req.cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK(req.msg_id == 5);
    CHECK(req.addr == 0x42);
    CHECK(req.len == 2);
    CHECK((req.data != nullptr) && (req.data[0] == 0xAA) &&
          (req.data[1] == 0xBB));

    stop(&fx);
}

/**
 * @brief Проверка события без ответа: пустой ответ по таймауту
 */
static void test_reply_timeout()
{
    Fixture fx;

    if (!start(&fx, "timeout",
               {"--semantic", "--no-config", "--reply-timeout", "50"}))
        return;

    send_to_adapter(&fx,
                    make_response(PROTO_CMD_SLAVE_EVENT, 9,
                                  PROTO_EVENT_FLAG_READ, 0x42,
                                  PROTO_STATUS_OK, nullptr, 0));

    CHECK(read_line(&fx) == "EVENT 9 R 42");
    CHECK(read_line(&fx, 1000) == "TIMEOUT");

    std::vector<uint8_t> raw;
    CHECK(read_frame(&fx, &raw));

    proto_req_t req;
    CHECK(parse_request(raw, &req));
    CHECK(req.cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK(req.msg_id == 9);
    CHECK(req.len == 0);

    stop(&fx);
}

/**
 * @brief Проверка ошибок клиента и разбора SLAVE_DONE
 */
static void test_client_errors_and_done()
{
    Fixture fx;

    if (!start(&fx, "errors",
               {"--semantic", "--no-config", "--reply-timeout", "1000"}))
        return;

    const uint8_t kDone[] = {0x02, 0x02};
    send_to_adapter(&fx, make_response(PROTO_CMD_SLAVE_DONE, 8,
                                       PROTO_EVENT_FLAG_WRITE |
                                           PROTO_EVENT_FLAG_READ,
                                       0x42, PROTO_STATUS_OK, kDone,
                                       sizeof(kDone)));
    CHECK(read_line(&fx) == "DONE W=2 R=2 00");

    send_line(&fx, "HELLO");
    CHECK(read_line(&fx) == "ERR CMD");

    send_line(&fx, "REPLY 1 AA");
    CHECK(read_line(&fx) == "ERR NOPENDING");

    send_to_adapter(&fx,
                    make_response(PROTO_CMD_SLAVE_EVENT, 12,
                                  PROTO_EVENT_FLAG_READ, 0x42,
                                  PROTO_STATUS_OK, nullptr, 0));
    CHECK(read_line(&fx) == "EVENT 12 R 42");

    send_line(&fx, "REPLY 11 AA");
    CHECK(read_line(&fx) == "ERR ID");

    send_line(&fx, "REPLY 12 ZZ");
    CHECK(read_line(&fx) == "ERR BADHEX");

    send_line(&fx, "REPLY 12");
    CHECK(read_line(&fx) == "OK");

    std::vector<uint8_t> raw;
    CHECK(read_frame(&fx, &raw));

    proto_req_t req;
    CHECK(parse_request(raw, &req));
    CHECK(req.msg_id == 12);
    CHECK(req.len == 0);

    stop(&fx);
}

/**
 * @brief Проверка прозрачного режима в обе стороны
 */
static void test_raw()
{
    Fixture fx;

    if (!start(&fx, "raw", {"--raw", "--no-config"}))
        return;

    const uint8_t kDown[] = {0x00, 0x11, 0x22, 0x00, 0x33};
    CHECK(send(fx.client, kDown, sizeof(kDown), 0) ==
          (ssize_t)sizeof(kDown));

    std::string got;
    long long deadline = now_ms() + kTimeoutMs;
    while ((got.size() < sizeof(kDown)) && (now_ms() < deadline))
    {
        struct pollfd pfd = {fx.master, POLLIN, 0};
        if (poll(&pfd, 1, 50) <= 0)
            continue;
        char chunk[64];
        ssize_t size = read(fx.master, chunk, sizeof(chunk));
        if (size <= 0)
            break;
        got.append(chunk, (size_t)size);
    }
    CHECK(got.size() == sizeof(kDown));
    CHECK(memcmp(got.data(), kDown, sizeof(kDown)) == 0);

    const uint8_t kUp[] = {0xA1, 0xB2, 0xC3};
    CHECK(write(fx.master, kUp, sizeof(kUp)) == (ssize_t)sizeof(kUp));

    struct pollfd pfd = {fx.client, POLLIN, 0};
    CHECK(poll(&pfd, 1, kTimeoutMs) == 1);

    uint8_t buf[16];
    ssize_t size = recv(fx.client, buf, sizeof(buf), 0);
    CHECK(size == (ssize_t)sizeof(kUp));
    CHECK(memcmp(buf, kUp, sizeof(kUp)) == 0);

    stop(&fx);
}

/**
 * @brief Проверка настройки режима при старте
 */
static void test_configuration()
{
    Fixture fx;

    if (!start(&fx, "config", {"--semantic", "--addr", "0x50"}, false))
        return;

    std::vector<uint8_t> raw;
    proto_req_t req;

    CHECK(read_frame(&fx, &raw));
    CHECK(parse_request(raw, &req));
    CHECK(req.cmd == PROTO_CMD_GET_CONFIG);

    const uint8_t kOld[] = {PROTO_MODE_MASTER, 0x42};
    send_to_adapter(&fx, make_response(PROTO_CMD_GET_CONFIG, req.msg_id, 0, 0,
                                       PROTO_STATUS_OK, kOld, sizeof(kOld)));

    CHECK(read_frame(&fx, &raw));
    CHECK(parse_request(raw, &req));
    CHECK(req.cmd == PROTO_CMD_SET_SLAVE_ADDR);
    CHECK((req.len == 1) && (req.data[0] == 0x50));
    send_to_adapter(&fx, make_response(PROTO_CMD_SET_SLAVE_ADDR, req.msg_id, 0,
                                       0, PROTO_STATUS_OK, nullptr, 0));

    CHECK(read_frame(&fx, &raw));
    CHECK(parse_request(raw, &req));
    CHECK(req.cmd == PROTO_CMD_SET_MODE);
    CHECK((req.len == 1) && (req.data[0] == PROTO_MODE_SLAVE));
    send_to_adapter(&fx, make_response(PROTO_CMD_SET_MODE, req.msg_id, 0, 0,
                                       PROTO_STATUS_OK, nullptr, 0));

    CHECK(read_frame(&fx, &raw));
    CHECK(parse_request(raw, &req));
    CHECK(req.cmd == PROTO_CMD_GET_CONFIG);

    const uint8_t kNew[] = {PROTO_MODE_SLAVE, 0x50};
    send_to_adapter(&fx, make_response(PROTO_CMD_GET_CONFIG, req.msg_id, 0, 0,
                                       PROTO_STATUS_OK, kNew, sizeof(kNew)));

    // Сокет открывается только после успешной настройки
    CHECK(connect_client(&fx));

    stop(&fx);
}
//<<----------------------

//>>---------------------- Public
int main()
{
    // Сокет создаётся утилитой; сигналы от убитого процесса не нужны
    (void)signal(SIGPIPE, SIG_IGN);

    test_event_and_reply();
    test_reply_timeout();
    test_client_errors_and_done();
    test_raw();
    test_configuration();

    if (m_failures != 0)
    {
        LOG_ERROR("adapter tests failed: %d checks", m_failures);
        return -1;
    }

    LOG_INFO("adapter tests passed");
    return 0;
}
//<<----------------------