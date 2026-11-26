/**
 * @file proto_test.cpp
 * @brief Юнит-тесты протокола host <-> I2C
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include <cstdint>
#include <cstring>
#include <vector>

#include "app/i2c/i2c_service.h"
#include "app/proto/frame.h"
#include "app/proto/link.h"
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

/** @brief Контекст канального уровня для тестов */
struct LinkCtx
{
    std::vector<uint8_t> out; /**< накопленный вывод */
    int busy_left; /**< сколько раз транспорт ответит "занят" */
    int handled; /**< сколько запросов обработано */
};

/** @brief Буфер ответа тестового обработчика */
static uint8_t m_resp_buf[PROTO_MAX_DATA];
/** @brief Контекст текущего теста для колбэка передачи */
static LinkCtx *m_capture;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Тестовый колбэк передачи
 *
 * @param data данные
 * @param size размер
 * @param ctx контекст (не используется, см. m_capture)
 * @return true - принято
 */
static bool capture_write(const uint8_t *data, uint32_t size, void *ctx)
{
    (void)ctx;
    LinkCtx *c = m_capture;

    if (c == nullptr)
        return false;

    if (c->busy_left > 0)
    {
        c->busy_left--;
        return false;
    }

    c->out.insert(c->out.end(), data, data + size);
    return true;
}

/**
 * @brief Тестовый обработчик: PING и эхо
 *
 * @param req запрос
 * @param resp ответ
 * @param ctx контекст
 */
static void echo_handler(const proto_req_t *req, proto_resp_t *resp, void *ctx)
{
    LinkCtx *c = static_cast<LinkCtx *>(ctx);
    c->handled++;

    if (req->cmd == PROTO_CMD_PING)
    {
        if (req->len != 0)
            memcpy(m_resp_buf, req->data, req->len);
        resp->len = req->len;
        resp->data = (req->len != 0) ? m_resp_buf : nullptr;
        return;
    }

    resp->status = PROTO_STATUS_NOT_SUPPORTED;
}

/**
 * @brief Инициализировать канальный уровень для теста
 *
 * @param link канальный уровень
 * @param ctx контекст теста
 * @param handler обработчик запросов
 */
static void init_link(proto_link_t *link, LinkCtx *ctx,
                      proto_handler_fn handler)
{
    m_capture = ctx;
    proto_link_init(link, capture_write, handler, ctx);
}

/**
 * @brief Сформировать сырой кадр запроса
 *
 * @param cmd команда
 * @param msg_id номер сообщения
 * @param flags флаги
 * @param addr адрес
 * @param len поле len
 * @param reg регистр
 * @param data payload
 * @param data_len длина payload
 * @return std::vector<uint8_t> кадр
 */
static std::vector<uint8_t> make_request(uint8_t cmd, uint8_t msg_id,
                                         uint8_t flags, uint8_t addr,
                                         uint8_t len, uint16_t reg,
                                         const uint8_t *data,
                                         uint32_t data_len)
{
    std::vector<uint8_t> raw(PROTO_REQ_HDR_SIZE + data_len + PROTO_CRC_SIZE);

    raw[0] = (uint8_t)((PROTO_VERSION << PROTO_VERSION_SHIFT) | cmd);
    raw[1] = msg_id;
    raw[2] = flags;
    raw[3] = addr;
    raw[4] = len;
    raw[5] = (uint8_t)(reg & 0xFF);
    raw[6] = (uint8_t)(reg >> 8);
    if (data_len != 0)
        memcpy(raw.data() + PROTO_REQ_HDR_SIZE, data, data_len);

    uint16_t crc =
        proto_crc16(raw.data(), PROTO_REQ_HDR_SIZE + data_len);
    raw[PROTO_REQ_HDR_SIZE + data_len] = (uint8_t)(crc & 0xFF);
    raw[PROTO_REQ_HDR_SIZE + data_len + 1] = (uint8_t)(crc >> 8);
    return raw;
}

/**
 * @brief Закодировать сырой кадр в COBS/R и добавить терминатор
 *
 * @param raw сырой кадр
 * @return std::vector<uint8_t> кадр на проводе
 */
static std::vector<uint8_t> encode_frame(const std::vector<uint8_t> &raw)
{
    uint8_t encoded[PROTO_MAX_COBS_FRAME];
    cobsr_encode_result result =
        cobsr_encode(encoded, sizeof(encoded), raw.data(), raw.size());

    if (result.status != COBSR_ENCODE_OK)
        return std::vector<uint8_t>();

    std::vector<uint8_t> wire(encoded, encoded + result.out_len);
    wire.push_back(0x00);
    return wire;
}

/**
 * @brief Разобрать кадр с провода без требования бита направления
 *
 * @param wire данные с провода
 * @param raw раскодированный кадр
 * @return true - успешно
 */
static bool decode_raw(const std::vector<uint8_t> &wire,
                       std::vector<uint8_t> *raw)
{
    uint8_t buf[PROTO_MAX_RAW_FRAME];

    if (wire.empty() || (wire.back() != 0x00))
        return false;

    cobsr_decode_result result =
        cobsr_decode(buf, sizeof(buf), wire.data(), wire.size() - 1);
    if (result.status != COBSR_DECODE_OK)
        return false;

    raw->assign(buf, buf + result.out_len);
    return true;
}

/**
 * @brief Разобрать кадр ответа с провода
 *
 * @param wire данные с провода
 * @param resp разобранный ответ
 * @param data payload ответа
 * @return true - успешно
 */
static bool parse_response(const std::vector<uint8_t> &wire,
                           proto_resp_t *resp, std::vector<uint8_t> *data)
{
    uint8_t raw[PROTO_MAX_RAW_FRAME];

    if (wire.empty() || (wire.back() != 0x00))
        return false;

    cobsr_decode_result result =
        cobsr_decode(raw, sizeof(raw), wire.data(), wire.size() - 1);
    if (result.status != COBSR_DECODE_OK)
        return false;

    uint32_t size = (uint32_t)result.out_len;
    if (size < PROTO_RESP_HDR_SIZE + PROTO_CRC_SIZE)
        return false;

    uint16_t crc = proto_crc16(raw, size - PROTO_CRC_SIZE);
    uint16_t frame_crc =
        (uint16_t)(raw[size - 2] | ((uint16_t)raw[size - 1] << 8));
    if (crc != frame_crc)
        return false;
    if ((raw[0] & PROTO_VERSION_MASK) !=
        (uint8_t)(PROTO_VERSION << PROTO_VERSION_SHIFT))
        return false;
    if ((raw[1] & PROTO_MSG_DIR_RESPONSE) == 0)
        return false;

    resp->cmd = (proto_cmd_t)(raw[0] & PROTO_CMD_MASK);
    resp->msg_id = raw[1] & PROTO_MSG_ID_MASK;
    resp->flags = raw[2];
    resp->addr = raw[3];
    resp->len = raw[4];
    resp->status = (proto_status_t)raw[5];

    if ((uint32_t)(PROTO_RESP_HDR_SIZE + resp->len + PROTO_CRC_SIZE) != size)
        return false;

    data->assign(raw + PROTO_RESP_HDR_SIZE,
                 raw + PROTO_RESP_HDR_SIZE + resp->len);
    resp->data = data->empty() ? nullptr : data->data();
    return true;
}

/**
 * @brief Подать сырой запрос в канальный уровень
 *
 * @param link канальный уровень
 * @param raw сырой кадр
 */
static void feed_request(proto_link_t *link, const std::vector<uint8_t> &raw)
{
    std::vector<uint8_t> wire = encode_frame(raw);
    proto_link_feed(link, wire.data(), (uint32_t)wire.size());
    proto_link_poll(link);
}
/**
 * @brief Сформировать сырой кадр ответного формата (устройство -> host)
 *
 * @param cmd команда
 * @param msg_id номер сообщения без бита направления
 * @param flags флаги
 * @param addr адрес
 * @param status статус
 * @param data payload
 * @param data_len длина payload
 * @return std::vector<uint8_t> кадр
 */
static std::vector<uint8_t> make_response(uint8_t cmd, uint8_t msg_id,
                                          uint8_t flags, uint8_t addr,
                                          uint8_t status,
                                          const uint8_t *data,
                                          uint32_t data_len)
{
    std::vector<uint8_t> raw(PROTO_RESP_HDR_SIZE + data_len + PROTO_CRC_SIZE);

    raw[0] = (uint8_t)((PROTO_VERSION << PROTO_VERSION_SHIFT) | cmd);
    raw[1] = (uint8_t)(msg_id | PROTO_MSG_DIR_RESPONSE);
    raw[2] = flags;
    raw[3] = addr;
    raw[4] = (uint8_t)data_len;
    raw[5] = status;
    if (data_len != 0)
        memcpy(raw.data() + PROTO_RESP_HDR_SIZE, data, data_len);

    uint16_t crc = proto_crc16(raw.data(), PROTO_RESP_HDR_SIZE + data_len);
    raw[PROTO_RESP_HDR_SIZE + data_len] = (uint8_t)(crc & 0xFF);
    raw[PROTO_RESP_HDR_SIZE + data_len + 1] = (uint8_t)(crc >> 8);
    return raw;
}

/**
 * @brief Пересчитать CRC после ручной правки кадра
 *
 * @param frame кадр без терминатора
 */
static void fix_crc(std::vector<uint8_t> *frame)
{
    uint16_t crc =
        proto_crc16(frame->data(), (uint32_t)frame->size() - PROTO_CRC_SIZE);
    (*frame)[frame->size() - 2] = (uint8_t)(crc & 0xFF);
    (*frame)[frame->size() - 1] = (uint8_t)(crc >> 8);
}
//<<----------------------

/**
 * @brief Проверка сборки запроса и его разбора
 */
static void test_build_request(void)
{
    const uint8_t kPayload[] = {0xAA, 0x00, 0xBB};
    proto_req_t req{};
    uint8_t buf[PROTO_MAX_RAW_FRAME];

    req.cmd = PROTO_CMD_WRITE;
    req.msg_id = 0x13;
    req.flags = 0x01;
    req.addr = 0x42;
    req.len = sizeof(kPayload);
    req.reg = 0x1234;
    req.data = kPayload;

    uint32_t size = proto_frame_build_req(buf, sizeof(buf), &req);
    CHECK(size == PROTO_REQ_HDR_SIZE + sizeof(kPayload) + PROTO_CRC_SIZE);

    proto_req_t parsed{};
    CHECK(proto_frame_parse(buf, size, &parsed) == PROTO_STATUS_OK);
    CHECK(parsed.cmd == PROTO_CMD_WRITE);
    CHECK(parsed.msg_id == 0x13);
    CHECK(parsed.flags == 0x01);
    CHECK(parsed.addr == 0x42);
    CHECK(parsed.reg == 0x1234);
    CHECK(parsed.len == sizeof(kPayload));
    CHECK((parsed.data != nullptr) &&
          (memcmp(parsed.data, kPayload, sizeof(kPayload)) == 0));

    // READ: len задаёт число читаемых байт, payload отсутствует
    proto_req_t read{};
    read.cmd = PROTO_CMD_READ;
    read.msg_id = 0x14;
    read.addr = 0x50;
    read.len = 0x04;
    size = proto_frame_build_req(buf, sizeof(buf), &read);
    CHECK(size == PROTO_REQ_HDR_SIZE + PROTO_CRC_SIZE);
    CHECK(buf[4] == 0x04);
    CHECK(proto_frame_parse(buf, size, &parsed) == PROTO_STATUS_OK);
    CHECK(parsed.len == 0x04);

    // ошибки сборки: нет места в буфере, нулевой payload, регистр не у READ
    CHECK(proto_frame_build_req(buf, PROTO_REQ_HDR_SIZE, &req) == 0);
    proto_req_t bad{};
    bad.cmd = PROTO_CMD_SLAVE_EVENT;
    bad.msg_id = 0x01;
    bad.flags = 0x01;
    bad.addr = 0x42;
    bad.len = 1;
    bad.data = nullptr;
    CHECK(proto_frame_build_req(buf, sizeof(buf), &bad) == 0);
}

/**
 * @brief Проверка разбора кадров от устройства (SLAVE_EVENT/SLAVE_DONE)
 */
static void test_parse_response(void)
{
    const uint8_t kWritten[] = {0x10, 0x20};
    proto_resp_t evt{};

    // SLAVE_EVENT: запись + ожидание чтения
    std::vector<uint8_t> event =
        make_response(PROTO_CMD_SLAVE_EVENT, 0x05,
                      PROTO_EVENT_FLAG_WRITE | PROTO_EVENT_FLAG_READ, 0x42,
                      PROTO_STATUS_OK, kWritten, sizeof(kWritten));
    CHECK(proto_frame_parse_response(event.data(), (uint32_t)event.size(),
                                     &evt) == PROTO_STATUS_OK);
    CHECK(evt.cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK(evt.msg_id == 0x05);
    CHECK(evt.flags == (PROTO_EVENT_FLAG_WRITE | PROTO_EVENT_FLAG_READ));
    CHECK(evt.addr == 0x42);
    CHECK(evt.status == PROTO_STATUS_OK);
    CHECK(evt.len == sizeof(kWritten));
    CHECK((evt.data != nullptr) &&
          (memcmp(evt.data, kWritten, sizeof(kWritten)) == 0));

    // SLAVE_DONE: две счётчика-байта
    const uint8_t kDone[] = {0x02, 0x02};
    std::vector<uint8_t> done =
        make_response(PROTO_CMD_SLAVE_DONE, 0x06,
                      PROTO_EVENT_FLAG_WRITE | PROTO_EVENT_FLAG_READ, 0x42,
                      PROTO_STATUS_OK, kDone, sizeof(kDone));
    CHECK(proto_frame_parse_response(done.data(), (uint32_t)done.size(),
                                     &evt) == PROTO_STATUS_OK);
    CHECK(evt.cmd == PROTO_CMD_SLAVE_DONE);
    CHECK(evt.len == PROTO_SLAVE_DONE_SIZE);

    // ответ на PING — тоже кадр ответного формата
    std::vector<uint8_t> ping =
        make_response(PROTO_CMD_PING, 0x07, 0x00, 0x00, PROTO_STATUS_OK,
                      nullptr, 0);
    CHECK(proto_frame_parse_response(ping.data(), (uint32_t)ping.size(),
                                     &evt) == PROTO_STATUS_OK);
    CHECK(evt.cmd == PROTO_CMD_PING);

    // ошибки: нет бита направления, битый CRC, лишняя длина,
    // неизвестная команда, неверная длина SLAVE_DONE
    std::vector<uint8_t> no_dir =
        make_response(PROTO_CMD_SLAVE_EVENT, 0x05, 0x00, 0x42,
                      PROTO_STATUS_OK, nullptr, 0);
    no_dir[1] &= (uint8_t)~PROTO_MSG_DIR_RESPONSE;
    fix_crc(&no_dir);
    CHECK(proto_frame_parse_response(no_dir.data(), (uint32_t)no_dir.size(),
                                     &evt) == PROTO_STATUS_FRAME);

    std::vector<uint8_t> bad_crc = event;
    bad_crc[bad_crc.size() - 1] ^= 0xFF;
    CHECK(proto_frame_parse_response(bad_crc.data(), (uint32_t)bad_crc.size(),
                                     &evt) == PROTO_STATUS_CRC);

    std::vector<uint8_t> long_frame =
        make_response(PROTO_CMD_SLAVE_EVENT, 0x08, 0x01, 0x42,
                      PROTO_STATUS_OK, kWritten, sizeof(kWritten));
    long_frame[4] = 0x03; // payload заявлен больше, чем в кадре
    fix_crc(&long_frame);
    CHECK(proto_frame_parse_response(long_frame.data(),
                                     (uint32_t)long_frame.size(), &evt) ==
          PROTO_STATUS_PARAM);

    std::vector<uint8_t> unknown =
        make_response(0x0F, 0x09, 0x00, 0x42, PROTO_STATUS_OK, nullptr, 0);
    CHECK(proto_frame_parse_response(unknown.data(), (uint32_t)unknown.size(),
                                     &evt) == PROTO_STATUS_UNKNOWN_CMD);

    std::vector<uint8_t> short_done =
        make_response(PROTO_CMD_SLAVE_DONE, 0x0A, 0x00, 0x42, PROTO_STATUS_OK,
                      nullptr, 0);
    CHECK(proto_frame_parse_response(short_done.data(),
                                     (uint32_t)short_done.size(), &evt) ==
          PROTO_STATUS_PARAM);
}

/**
 * @brief Проверка перенаправления кадров от устройства обработчику событий
 */
static void test_event_handler_dispatch(void)
{
    LinkCtx ctx{};
    proto_link_t link;
    static int s_events;
    static proto_cmd_t s_cmd;

    s_events = 0;
    s_cmd = PROTO_CMD_PING;

    auto on_event = [](const proto_resp_t *evt, void *arg) {
        (void)arg;
        s_events++;
        s_cmd = evt->cmd;
    };

    init_link(&link, &ctx, echo_handler);
    proto_link_set_event_handler(&link, on_event);

    const uint8_t kWritten[] = {0x01};
    std::vector<uint8_t> event =
        make_response(PROTO_CMD_SLAVE_EVENT, 0x11,
                      PROTO_EVENT_FLAG_WRITE | PROTO_EVENT_FLAG_READ, 0x42,
                      PROTO_STATUS_OK, kWritten, sizeof(kWritten));
    std::vector<uint8_t> wire = encode_frame(event);
    proto_link_feed(&link, wire.data(), (uint32_t)wire.size());

    CHECK(s_events == 1);
    CHECK(s_cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK(ctx.handled == 0); // обработчик запросов не вызывался
    CHECK(ctx.out.empty());  // и ответ никуда не ушёл

    // запросы по-прежнему обрабатываются как запросы
    feed_request(&link, make_request(PROTO_CMD_PING, 0x12, 0x00, 0x00, 0x00,
                                     0, nullptr, 0));
    CHECK(ctx.handled == 1);
    CHECK(s_events == 1);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.msg_id == 0x12);
}

/**
 * @brief Проверка постановки произвольного кадра в очередь передачи
 */
static void test_send_frame_roundtrip(void)
{
    LinkCtx ctx{};
    proto_link_t link;
    init_link(&link, &ctx, nullptr);

    const uint8_t kRead[] = {0xAA, 0xBB};
    std::vector<uint8_t> raw =
        make_request(PROTO_CMD_SLAVE_EVENT, 0x21, 0x00, 0x42, sizeof(kRead), 0,
                     kRead, sizeof(kRead));

    CHECK(proto_link_send_frame(&link, raw.data(), (uint32_t)raw.size()));
    proto_link_poll(&link);

    CHECK(ctx.out == encode_frame(raw));

    std::vector<uint8_t> decoded;
    CHECK(decode_raw(ctx.out, &decoded));
    CHECK(decoded == raw);

    // ответ хоста на SLAVE_EVENT — кадр запросного формата
    proto_req_t req{};
    CHECK(proto_frame_parse(decoded.data(), (uint32_t)decoded.size(), &req) ==
          PROTO_STATUS_OK);
    CHECK(req.cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK(req.msg_id == 0x21);
    CHECK(req.len == sizeof(kRead));
    CHECK((req.data != nullptr) && (memcmp(req.data, kRead, sizeof(kRead)) == 0));
}

//>>---------------------- Private tests
/**
 * @brief Проверка известного вектора CRC-16/CCITT-FALSE
 */
static void test_crc_vector(void)
{
    static const uint8_t kData[] = "123456789";
    CHECK(proto_crc16(kData, sizeof(kData) - 1) == 0x29B1);
}

/**
 * @brief Проверка разбора READ
 */
static void test_parse_read(void)
{
    std::vector<uint8_t> raw =
        make_request(PROTO_CMD_READ, 0x05, 0x02, 0x50, 0x02, 0x1234,
                     nullptr, 0);
    proto_req_t req;

    CHECK(raw.size() == PROTO_REQ_HDR_SIZE + PROTO_CRC_SIZE);
    CHECK(proto_frame_parse(raw.data(), (uint32_t)raw.size(), &req) ==
          PROTO_STATUS_OK);
    CHECK(req.cmd == PROTO_CMD_READ);
    CHECK(req.msg_id == 0x05);
    CHECK(req.flags == 0x02);
    CHECK(req.addr == 0x50);
    CHECK(req.len == 0x02);
    CHECK(req.reg == 0x1234);
    CHECK(req.data == nullptr);
}

/**
 * @brief Проверка ошибок разбора
 */
static void test_parse_errors(void)
{
    proto_req_t req;

    // битый CRC
    std::vector<uint8_t> bad_crc =
        make_request(PROTO_CMD_READ, 0x01, 0x00, 0x50, 0x01, 0, nullptr, 0);
    bad_crc[bad_crc.size() - 1] ^= 0xFF;
    CHECK(proto_frame_parse(bad_crc.data(), (uint32_t)bad_crc.size(), &req) ==
          PROTO_STATUS_CRC);

    // неверная версия
    std::vector<uint8_t> bad_version =
        make_request(PROTO_CMD_READ, 0x01, 0x00, 0x50, 0x01, 0, nullptr, 0);
    bad_version[0] = (uint8_t)(((PROTO_VERSION + 1) << PROTO_VERSION_SHIFT) |
                              PROTO_CMD_READ);
    CHECK(proto_frame_parse(bad_version.data(),
                            (uint32_t)bad_version.size(), &req) ==
          PROTO_STATUS_FRAME);

    // зарезервированные флаги
    std::vector<uint8_t> bad_flags =
        make_request(PROTO_CMD_READ, 0x01, 0x10, 0x50, 0x01, 0, nullptr, 0);
    CHECK(proto_frame_parse(bad_flags.data(), (uint32_t)bad_flags.size(),
                            &req) == PROTO_STATUS_PARAM);

    // неизвестная команда
    std::vector<uint8_t> unknown =
        make_request(0x0F, 0x01, 0x00, 0x50, 0x00, 0, nullptr, 0);
    CHECK(proto_frame_parse(unknown.data(), (uint32_t)unknown.size(), &req) ==
          PROTO_STATUS_UNKNOWN_CMD);

    // направление ответа в запросе
    std::vector<uint8_t> response_dir =
        make_request(PROTO_CMD_READ, 0x81, 0x00, 0x50, 0x01, 0, nullptr, 0);
    CHECK(proto_frame_parse(response_dir.data(),
                            (uint32_t)response_dir.size(), &req) ==
          PROTO_STATUS_FRAME);

    // неверная длина кадра
    std::vector<uint8_t> short_frame =
        make_request(PROTO_CMD_WRITE, 0x01, 0x00, 0x50, 0x02, 0, nullptr, 0);
    CHECK(proto_frame_parse(short_frame.data(), (uint32_t)short_frame.size(),
                            &req) == PROTO_STATUS_PARAM);

    // некорректный диапазон SCAN
    static const uint8_t kRange[] = {0x50, 0x10};
    std::vector<uint8_t> scan_range = make_request(
        PROTO_CMD_SCAN, 0x01, 0x00, 0x50, 0x02, 0, kRange, 2);
    CHECK(proto_frame_parse(scan_range.data(), (uint32_t)scan_range.size(),
                            &req) == PROTO_STATUS_PARAM);
}

/**
 * @brief Проверка round-trip PING с нулями в payload
 */
static void test_ping_roundtrip(void)
{
    static const uint8_t kPayload[] = {0x00, 0x11, 0x00, 0x22, 0x00};
    LinkCtx ctx{};
    proto_link_t link;

    init_link(&link, &ctx, echo_handler);

    std::vector<uint8_t> raw = make_request(
        PROTO_CMD_PING, 0x02, 0x00, 0x00, sizeof(kPayload), 0, kPayload,
        sizeof(kPayload));
    feed_request(&link, raw);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(ctx.handled == 1);
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.cmd == PROTO_CMD_PING);
    CHECK(resp.msg_id == 0x02);
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(resp.len == sizeof(kPayload));
    CHECK(data == std::vector<uint8_t>(kPayload, kPayload + sizeof(kPayload)));
}

/**
 * @brief Проверка максимального payload (128 байт)
 */
static void test_max_payload(void)
{
    std::vector<uint8_t> payload(PROTO_MAX_DATA);
    for (uint32_t i = 0; i < payload.size(); ++i)
        payload[i] = (uint8_t)(i * 3);

    LinkCtx ctx{};
    proto_link_t link;
    init_link(&link, &ctx, echo_handler);

    std::vector<uint8_t> raw = make_request(
        PROTO_CMD_PING, 0x03, 0x00, 0x00, PROTO_MAX_DATA, 0, payload.data(),
        (uint32_t)payload.size());
    feed_request(&link, raw);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(data == payload);
}

/**
 * @brief Проверка потокового приёма, мусора и повторных терминаторов
 */
static void test_stream_and_resync(void)
{
    LinkCtx ctx{};
    proto_link_t link;
    init_link(&link, &ctx, echo_handler);

    std::vector<uint8_t> raw =
        make_request(PROTO_CMD_PING, 0x04, 0x00, 0x00, 0x00, 0, nullptr, 0);
    std::vector<uint8_t> wire = encode_frame(raw);

    // мусор и повторные терминаторы
    static const uint8_t kGarbage[] = {0x00, 0x00, 0xAA, 0x55, 0x00};
    proto_link_feed(&link, kGarbage, sizeof(kGarbage));

    // кадр приходит двумя частями
    uint32_t split = (uint32_t)wire.size() / 2;
    proto_link_feed(&link, wire.data(), split);
    proto_link_feed(&link, wire.data() + split,
                    (uint32_t)wire.size() - split);
    proto_link_poll(&link);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(ctx.handled == 1);
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.msg_id == 0x04);
}

/**
 * @brief Проверка переполнения приёмного буфера и ресинхронизации
 */
static void test_overflow_resync(void)
{
    LinkCtx ctx{};
    proto_link_t link;
    init_link(&link, &ctx, echo_handler);

    std::vector<uint8_t> junk(PROTO_MAX_COBS_FRAME + 10, 0xAA);
    proto_link_feed(&link, junk.data(), (uint32_t)junk.size());
    static const uint8_t kDelimiter = 0x00;
    proto_link_feed(&link, &kDelimiter, 1);

    CHECK(proto_link_stats(&link)->rx_overflow == 1);

    std::vector<uint8_t> raw =
        make_request(PROTO_CMD_PING, 0x06, 0x00, 0x00, 0x00, 0, nullptr, 0);
    feed_request(&link, raw);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.msg_id == 0x06);
}

/**
 * @brief Проверка ответа на битый CRC (ответа быть не должно)
 */
static void test_bad_crc_silent(void)
{
    LinkCtx ctx{};
    proto_link_t link;
    init_link(&link, &ctx, echo_handler);

    std::vector<uint8_t> raw =
        make_request(PROTO_CMD_PING, 0x07, 0x00, 0x00, 0x00, 0, nullptr, 0);
    raw[raw.size() - 2] ^= 0x01;
    feed_request(&link, raw);

    CHECK(ctx.out.empty());
    CHECK(ctx.handled == 0);
    CHECK(proto_link_stats(&link)->rx_crc_err == 1);
}

/**
 * @brief Проверка ошибки неизвестной команды с ответом
 */
static void test_unknown_cmd_response(void)
{
    LinkCtx ctx{};
    proto_link_t link;
    init_link(&link, &ctx, echo_handler);

    std::vector<uint8_t> raw =
        make_request(0x0F, 0x08, 0x00, 0x50, 0x00, 0, nullptr, 0);
    feed_request(&link, raw);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.msg_id == 0x08);
    CHECK(resp.cmd == (proto_cmd_t)0x0F);
    CHECK(resp.status == PROTO_STATUS_UNKNOWN_CMD);
}

/**
 * @brief Проверка повтора передачи при занятом транспорте
 */
static void test_tx_busy_retry(void)
{
    LinkCtx ctx{};
    proto_link_t link;
    init_link(&link, &ctx, echo_handler);

    ctx.busy_left = 3;

    std::vector<uint8_t> raw =
        make_request(PROTO_CMD_PING, 0x09, 0x00, 0x00, 0x00, 0, nullptr, 0);
    std::vector<uint8_t> wire = encode_frame(raw);
    proto_link_feed(&link, wire.data(), (uint32_t)wire.size());

    for (int i = 0; i < 5; ++i)
        proto_link_poll(&link);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.msg_id == 0x09);
}

/**
 * @brief Проверка сервиса I2C на эмуляции шины
 */
static void test_i2c_service(void)
{
    LinkCtx ctx{};
    proto_link_t link;

    m_capture = &ctx;
    proto_link_init(&link, capture_write, i2c_service_handle, &link);
    i2c_service_init(&link);

    static const uint8_t kData[] = {0xDE, 0xAD, 0x00, 0xBE, 0xEF};
    const uint8_t kAddr = 0x0C;
    const uint16_t kReg = 0x10;
    const uint8_t kRegSize = 1;

    std::vector<uint8_t> write_req =
        make_request(PROTO_CMD_WRITE, 0x11, kRegSize, kAddr, sizeof(kData),
                     kReg, kData, sizeof(kData));
    feed_request(&link, write_req);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(resp.len == 0);

    ctx.out.clear();
    std::vector<uint8_t> read_req =
        make_request(PROTO_CMD_READ, 0x12, kRegSize, kAddr, sizeof(kData),
                     kReg, nullptr, 0);
    feed_request(&link, read_req);

    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(data == std::vector<uint8_t>(kData, kData + sizeof(kData)));

    // NACK на несуществующем устройстве
    ctx.out.clear();
    std::vector<uint8_t> nack_req =
        make_request(PROTO_CMD_READ, 0x13, 0x00, 0x33, 0x01, 0, nullptr, 0);
    feed_request(&link, nack_req);

    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_NACK_ADDR);
}

/**
 * @brief Проверка INFO, STATS и BUS_RECOVER через сервис
 */
static void test_i2c_info_stats(void)
{
    LinkCtx ctx{};
    proto_link_t link;

    m_capture = &ctx;
    proto_link_init(&link, capture_write, i2c_service_handle, &link);
    i2c_service_init(&link);

    proto_resp_t resp{};
    std::vector<uint8_t> data;

    feed_request(&link, make_request(PROTO_CMD_INFO, 0x31, 0x00, 0x00, 0x00,
                                     0, nullptr, 0));
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(data.size() == PROTO_INFO_SIZE);
    CHECK(data[0] == PROTO_VERSION);
    CHECK(data[2] == PROTO_MAX_DATA);

    ctx.out.clear();
    feed_request(&link, make_request(PROTO_CMD_STATS, 0x32, 0x00, 0x00, 0x00,
                                     0, nullptr, 0));
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(data.size() == PROTO_STATS_SIZE);

    ctx.out.clear();
    feed_request(&link, make_request(PROTO_CMD_BUS_RECOVER, 0x33, 0x00, 0x00,
                                     0x00, 0, nullptr, 0));
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
}

/**
 * @brief Проверка SCAN через сервис
 */
static void test_i2c_scan(void)
{
    LinkCtx ctx{};
    proto_link_t link;

    m_capture = &ctx;
    proto_link_init(&link, capture_write, i2c_service_handle, &link);
    i2c_service_init(&link);

    static const uint8_t kRange[] = {0x00, 0x7F};
    std::vector<uint8_t> scan_req = make_request(
        PROTO_CMD_SCAN, 0x21, 0x00, 0x00, 0x02, 0, kRange, 2);
    feed_request(&link, scan_req);

    proto_resp_t resp{};
    std::vector<uint8_t> data;
    CHECK(parse_response(ctx.out, &resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(data.size() == PROTO_SCAN_BITMAP_SIZE);
    CHECK((data[0x0C >> 3] & (1u << (0x0C & 0x07))) != 0);
    CHECK((data[0x50 >> 3] & (1u << (0x50 & 0x07))) != 0);
    CHECK((data[0x0D >> 3] & (1u << (0x0D & 0x07))) == 0);
}
//<<----------------------

//>>---------------------- Public
int main()
{
    test_crc_vector();
    test_parse_read();
    test_parse_errors();
    test_build_request();
    test_parse_response();
    test_event_handler_dispatch();
    test_send_frame_roundtrip();
    test_ping_roundtrip();
    test_max_payload();
    test_stream_and_resync();
    test_overflow_resync();
    test_bad_crc_silent();
    test_unknown_cmd_response();
    test_tx_busy_retry();
    test_i2c_service();
    test_i2c_info_stats();
    test_i2c_scan();

    if (m_failures != 0)
    {
        LOG_ERROR("proto tests failed: %d checks", m_failures);
        return -1;
    }

    LOG_INFO("proto tests passed");
    return 0;
}
//<<----------------------