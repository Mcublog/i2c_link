/**
 * @file main.c
 * @brief i2c-link-adapter: мост между UNIX-сокетом и CDC-портом платы
 * @version 0.1
 * @date 2026-11-26
 *
 */
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "app/proto/link.h"
#include "app/proto/proto.h"
#include "byte_queue.h"
#include "cdc_port.h"
#include "cli.h"
#include "client_server.h"
#include "semantic.h"
#include "session.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME adapt
#define LOG_MODULE_LEVEL (4)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Ёмкость очереди в CDC, байт */
#define TTY_TX_CAPACITY (4096)
/** @brief Ёмкость очереди клиенту, байт */
#define CLIENT_TX_CAPACITY (8192)
/** @brief Размер порции чтения, байт */
#define READ_CHUNK_SIZE (256)
/** @brief Размер неделимой порции записи, байт */
#define WRITE_CHUNK_SIZE (1024)
/** @brief Период переоткрытия CDC-порта, мс */
#define TTY_RETRY_PERIOD_MS (500)
/** @brief Базовый период цикла, мс */
#define LOOP_PERIOD_MS (100)

/**
 * @brief Состояние утилиты
 */
typedef struct
{
    cli_opts_t opts; /**< параметры запуска */
    cdc_port_t tty; /**< CDC-порт платы */
    client_server_t server; /**< UNIX-сокет-сервер */
    proto_link_t link; /**< канальный уровень протокола */
    session_t session; /**< настройка режима */
    semantic_t semantic; /**< семантический режим */
    byte_queue_t tty_tx; /**< очередь в CDC */
    byte_queue_t client_tx; /**< очередь клиенту */
    uint8_t tty_tx_storage[TTY_TX_CAPACITY]; /**< память очереди в CDC */
    uint8_t client_tx_storage[CLIENT_TX_CAPACITY]; /**< память клиенту */
    uint8_t read_chunk[READ_CHUNK_SIZE]; /**< порция чтения */
    uint8_t tty_write[WRITE_CHUNK_SIZE]; /**< текущая порция в CDC */
    uint32_t tty_write_len; /**< длина порции в CDC */
    uint32_t tty_write_off; /**< отправлено из порции в CDC */
    uint8_t client_write[WRITE_CHUNK_SIZE]; /**< текущая порция клиенту */
    uint32_t client_write_len; /**< длина порции клиенту */
    uint32_t client_write_off; /**< отправлено из порции клиенту */
    uint64_t tty_retry_at; /**< когда пробовать открыть CDC снова */
    uint64_t listen_retry_at; /**< когда пробовать открыть сокет снова */
    bool listener_started; /**< сокет уже слушает */
} adapter_t;

/** @brief Единственный экземпляр утилиты */
static adapter_t m_adapter;
/** @brief Признак запроса на выход */
static volatile sig_atomic_t m_stop;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Обработчик сигналов завершения
 *
 * @param signal номер сигнала
 */
static void on_signal(int signal_number)
{
    (void)signal_number;
    m_stop = 1;
}

/**
 * @brief Монотонное время, мс
 *
 * @return uint64_t миллисекунды
 */
static uint64_t now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;

    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

/**
 * @brief Колбэк передачи канального уровня в CDC
 *
 * @param data данные
 * @param size размер данных
 * @param ctx контекст (adapter_t)
 * @return true - данные поставлены в очередь
 */
static bool link_write(const uint8_t *data, uint32_t size, void *ctx)
{
    adapter_t *adapter = (adapter_t *)ctx;

    return byte_queue_push(&adapter->tty_tx, data, size);
}

/**
 * @brief Колбэк отправки сырого кадра от сеанса настройки
 *
 * @param ctx контекст (adapter_t)
 * @param raw кадр
 * @param size размер кадра
 * @return true - кадр поставлен в очередь
 */
static bool session_send(void *ctx, const uint8_t *raw, uint32_t size)
{
    adapter_t *adapter = (adapter_t *)ctx;

    return proto_link_send_frame(&adapter->link, raw, size);
}

/**
 * @brief Колбэк отправки сырого кадра от семантического режима
 *
 * @param ctx контекст (adapter_t)
 * @param raw кадр
 * @param size размер кадра
 * @return true - кадр поставлен в очередь
 */
static bool semantic_send(void *ctx, const uint8_t *raw, uint32_t size)
{
    adapter_t *adapter = (adapter_t *)ctx;

    return proto_link_send_frame(&adapter->link, raw, size);
}

/**
 * @brief Колбэк отправки текста клиенту
 *
 * @param ctx контекст (adapter_t)
 * @param text строка
 * @param size длина строки
 * @return true - строка поставлена в очередь
 */
static bool client_text(void *ctx, const char *text, uint32_t size)
{
    adapter_t *adapter = (adapter_t *)ctx;

    return byte_queue_push(&adapter->client_tx, (const uint8_t *)text, size);
}

/**
 * @brief Колбэк кадров от устройства
 *
 * @param evt разобранный кадр
 * @param ctx контекст (adapter_t)
 */
static void on_event(const proto_resp_t *evt, void *ctx)
{
    adapter_t *adapter = (adapter_t *)ctx;
    uint64_t now = now_ms();

    session_on_frame(&adapter->session, evt, now);

    if (adapter->opts.semantic)
        semantic_on_frame(&adapter->semantic, evt, now);
}

/**
 * @brief Сбросить канальный уровень после переоткрытия CDC
 *
 * @param adapter состояние
 */
static void reset_link(adapter_t *adapter)
{
    proto_link_init(&adapter->link, link_write, NULL, adapter);
    proto_link_set_event_handler(&adapter->link, on_event);
    byte_queue_clear(&adapter->tty_tx);
    adapter->tty_write_len = 0;
    adapter->tty_write_off = 0;
}

/**
 * @brief Открыть CDC-порт, если он ещё не открыт
 *
 * @param adapter состояние
 */
static void try_open_tty(adapter_t *adapter)
{
    uint64_t now = now_ms();

    if (cdc_port_is_open(&adapter->tty))
        return;
    if (now < adapter->tty_retry_at)
        return;

    if (!cdc_port_open(&adapter->tty))
    {
        adapter->tty_retry_at = now + TTY_RETRY_PERIOD_MS;
        return;
    }

    reset_link(adapter);
    semantic_drop(&adapter->semantic);

    if (!adapter->opts.configure)
    {
        session_start(&adapter->session, now);
        return;
    }

    if (!adapter->opts.semantic &&
        client_server_connected(&adapter->server))
    {
        // NOTE: в прозрачном режиме клиент владеет каналом, поэтому
        // настройку не выполняем, чтобы не подмешивать свои кадры
        LOG_WARNING("raw client connected: startup configuration skipped");
        session_abort(&adapter->session);
        return;
    }

    session_start(&adapter->session, now);
}

/**
 * @brief Обработать потерю CDC-порта
 *
 * @param adapter состояние
 */
static void tty_lost(adapter_t *adapter)
{
    LOG_WARNING("CDC port lost");

    cdc_port_close(&adapter->tty);
    adapter->tty_retry_at = now_ms() + TTY_RETRY_PERIOD_MS;

    reset_link(adapter);
    semantic_drop(&adapter->semantic);
    session_abort(&adapter->session);
}

/**
 * @brief Прочитать порцию из CDC и раздать её потребителям
 *
 * @param adapter состояние
 */
static void read_tty(adapter_t *adapter)
{
    for (uint32_t i = 0; i < 8; ++i)
    {
        int32_t size = cdc_port_read(&adapter->tty, adapter->read_chunk,
                                     sizeof(adapter->read_chunk));
        if (size < 0)
        {
            tty_lost(adapter);
            return;
        }
        if (size == 0)
            return;

        if (adapter->opts.semantic || session_active(&adapter->session))
        {
            proto_link_feed(&adapter->link, adapter->read_chunk,
                            (uint32_t)size);
        }
        else if (!client_server_connected(&adapter->server))
        {
            // NOTE: в прозрачном режиме данные некому отдавать
            continue;
        }
        else if (!byte_queue_push(&adapter->client_tx, adapter->read_chunk,
                                  (uint32_t)size))
        {
            LOG_WARNING("client queue full, %d bytes dropped", (int)size);
        }
    }
}

/**
 * @brief Прочитать порцию от клиента и раздать её потребителям
 *
 * @param adapter состояние
 */
static void read_client(adapter_t *adapter)
{
    for (uint32_t i = 0; i < 8; ++i)
    {
        int32_t size = client_server_read(&adapter->server,
                                          adapter->read_chunk,
                                          sizeof(adapter->read_chunk));
        if (size < 0)
        {
            client_server_drop(&adapter->server);
            if (adapter->opts.semantic)
                semantic_cancel(&adapter->semantic);
            byte_queue_clear(&adapter->client_tx);
            adapter->client_write_len = 0;
            adapter->client_write_off = 0;
            return;
        }
        if (size == 0)
            return;

        if (adapter->opts.semantic)
        {
            semantic_feed(&adapter->semantic, adapter->read_chunk,
                          (uint32_t)size, now_ms());
        }
        else if (!byte_queue_push(&adapter->tty_tx, adapter->read_chunk,
                                  (uint32_t)size))
        {
            LOG_WARNING("cdc queue full, %d bytes dropped", (int)size);
        }
    }
}

/**
 * @brief Отправить накопленное в CDC
 *
 * @param adapter состояние
 * @return false - CDC потерян
 */
static bool flush_tty(adapter_t *adapter)
{
    if (!cdc_port_is_open(&adapter->tty))
        return true;

    for (;;)
    {
        if (adapter->tty_write_len == 0)
        {
            adapter->tty_write_off = 0;
            adapter->tty_write_len = byte_queue_pop(
                &adapter->tty_tx, adapter->tty_write,
                sizeof(adapter->tty_write));
            if (adapter->tty_write_len == 0)
                return true;
        }

        int32_t written = cdc_port_write(
            &adapter->tty, adapter->tty_write + adapter->tty_write_off,
            adapter->tty_write_len - adapter->tty_write_off);
        if (written < 0)
            return false;
        if (written == 0)
            return true;

        adapter->tty_write_off += (uint32_t)written;
        if (adapter->tty_write_off >= adapter->tty_write_len)
        {
            adapter->tty_write_len = 0;
            adapter->tty_write_off = 0;
        }
    }
}

/**
 * @brief Отправить накопленное клиенту
 *
 * @param adapter состояние
 * @return false - клиент отключился
 */
static bool flush_client(adapter_t *adapter)
{
    if (!client_server_connected(&adapter->server))
        return true;

    for (;;)
    {
        if (adapter->client_write_len == 0)
        {
            adapter->client_write_off = 0;
            adapter->client_write_len = byte_queue_pop(
                &adapter->client_tx, adapter->client_write,
                sizeof(adapter->client_write));
            if (adapter->client_write_len == 0)
                return true;
        }

        int32_t written = client_server_write(
            &adapter->server,
            adapter->client_write + adapter->client_write_off,
            adapter->client_write_len - adapter->client_write_off);
        if (written < 0)
            return false;
        if (written == 0)
            return true;

        adapter->client_write_off += (uint32_t)written;
        if (adapter->client_write_off >= adapter->client_write_len)
        {
            adapter->client_write_len = 0;
            adapter->client_write_off = 0;
        }
    }
}

/**
 * @brief Принять клиента и привести состояние в соответствие
 *
 * @param adapter состояние
 */
static void handle_accept(adapter_t *adapter)
{
    client_accept_t result = client_server_accept(&adapter->server);

    if (result != CLIENT_ACCEPT_NEW)
        return;

    byte_queue_clear(&adapter->client_tx);
    adapter->client_write_len = 0;
    adapter->client_write_off = 0;

    if (adapter->opts.semantic)
        return;

    if (session_active(&adapter->session))
    {
        // NOTE: прозрачный клиент начинает говорить на протоколе сам
        LOG_WARNING("client connected during configuration: client owns link");
        session_abort(&adapter->session);
        reset_link(adapter);
    }
}

/**
 * @brief Запустить сокет-сервер, когда режим устройства определён
 *
 * @param adapter состояние
 */
static void maybe_start_listener(adapter_t *adapter)
{
    if (adapter->listener_started)
        return;
    if (!cdc_port_is_open(&adapter->tty))
        return;
    if (session_active(&adapter->session))
        return;

    uint64_t now = now_ms();
    if (now < adapter->listen_retry_at)
        return;

    if (!client_server_start(&adapter->server))
    {
        adapter->listen_retry_at = now + 1000;
        return;
    }

    adapter->listener_started = true;
}

/**
 * @brief Вычислить таймаут poll() до ближайшего события
 *
 * @param adapter состояние
 * @return int таймаут, мс
 */
static int compute_timeout(const adapter_t *adapter)
{
    uint64_t now = now_ms();
    uint64_t deadline = now + LOOP_PERIOD_MS;

    if (session_active(&adapter->session) &&
        (adapter->session.deadline_ms < deadline))
        deadline = adapter->session.deadline_ms;

    if (semantic_pending(&adapter->semantic) &&
        (adapter->semantic.reply_timeout_ms != 0) &&
        (adapter->semantic.deadline_ms < deadline))
        deadline = adapter->semantic.deadline_ms;

    if (!cdc_port_is_open(&adapter->tty) &&
        (adapter->tty_retry_at < deadline))
        deadline = adapter->tty_retry_at;

    if (deadline <= now)
        return 0;

    uint64_t diff = deadline - now;
    return (diff > LOOP_PERIOD_MS) ? LOOP_PERIOD_MS : (int)diff;
}
//<<----------------------

//>>---------------------- Public
int main(int argc, char **argv)
{
    adapter_t *adapter = &m_adapter;
    bool need_exit = false;

    if (!cli_parse(argc, argv, &adapter->opts, &need_exit))
        return need_exit ? 0 : 1;

    (void)signal(SIGPIPE, SIG_IGN);
    (void)signal(SIGINT, on_signal);
    (void)signal(SIGTERM, on_signal);

    if (!cdc_port_init(&adapter->tty, adapter->opts.tty))
    {
        LOG_ERROR("bad tty path");
        return 1;
    }

    if (!client_server_init(&adapter->server, adapter->opts.listen))
    {
        LOG_ERROR("bad socket path");
        return 1;
    }

    byte_queue_init(&adapter->tty_tx, adapter->tty_tx_storage,
                    sizeof(adapter->tty_tx_storage));
    byte_queue_init(&adapter->client_tx, adapter->client_tx_storage,
                    sizeof(adapter->client_tx_storage));

    reset_link(adapter);
    session_init(&adapter->session, adapter->opts.configure,
                 adapter->opts.mode, adapter->opts.addr, session_send,
                 adapter);
    semantic_init(&adapter->semantic, adapter->opts.reply_timeout_ms,
                  client_text, semantic_send, adapter);

    LOG_INFO("adapter started: tty=%s socket=%s mode=%s%s", adapter->opts.tty,
             adapter->opts.listen,
             (adapter->opts.mode == PROTO_MODE_SLAVE) ? "slave" : "master",
             adapter->opts.semantic ? ", semantic" : ", raw");

    while (!m_stop)
    {
        try_open_tty(adapter);

        struct pollfd fds[3];
        nfds_t count = 0;
        int listen_index = -1;
        int client_index = -1;
        int tty_index = -1;

        if (adapter->server.listen_fd >= 0)
        {
            listen_index = (int)count;
            fds[count].fd = adapter->server.listen_fd;
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }

        if (client_server_connected(&adapter->server))
        {
            client_index = (int)count;
            fds[count].fd = adapter->server.client_fd;
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }

        if (cdc_port_is_open(&adapter->tty))
        {
            tty_index = (int)count;
            fds[count].fd = adapter->tty.fd;
            fds[count].events = POLLIN;
            fds[count].revents = 0;
            count++;
        }

        int ready = poll(fds, count, compute_timeout(adapter));
        if ((ready < 0) && (m_stop == 0))
            LOG_WARNING("poll failed");

        if ((listen_index >= 0) && (fds[listen_index].revents & POLLIN) != 0)
            handle_accept(adapter);

        if ((client_index >= 0) && (fds[client_index].revents != 0))
        {
            if ((fds[client_index].revents & (POLLERR | POLLHUP)) != 0)
            {
                client_server_drop(&adapter->server);
                if (adapter->opts.semantic)
                    semantic_cancel(&adapter->semantic);
                byte_queue_clear(&adapter->client_tx);
                adapter->client_write_len = 0;
                adapter->client_write_off = 0;
            }
            else if ((fds[client_index].revents & POLLIN) != 0)
            {
                read_client(adapter);
            }
        }

        if ((tty_index >= 0) && (fds[tty_index].revents != 0))
        {
            // NOTE: при POLLIN|POLLHUP сначала дочитываем буфер, потом
            // считаем порт потерянным
            if ((fds[tty_index].revents & POLLIN) != 0)
                read_tty(adapter);

            if (((fds[tty_index].revents & (POLLERR | POLLHUP)) != 0) &&
                cdc_port_is_open(&adapter->tty))
                tty_lost(adapter);
        }

        if (cdc_port_is_open(&adapter->tty))
        {
            session_poll(&adapter->session, now_ms());
            if (adapter->opts.semantic)
                semantic_poll(&adapter->semantic, now_ms());
            proto_link_poll(&adapter->link);
        }

        if (!flush_tty(adapter))
            tty_lost(adapter);

        if (!flush_client(adapter))
        {
            client_server_drop(&adapter->server);
            if (adapter->opts.semantic)
                semantic_cancel(&adapter->semantic);
            byte_queue_clear(&adapter->client_tx);
            adapter->client_write_len = 0;
            adapter->client_write_off = 0;
        }

        maybe_start_listener(adapter);
    }

    LOG_INFO("adapter stopped");
    client_server_stop(&adapter->server);
    cdc_port_close(&adapter->tty);
    return 0;
}
//<<----------------------