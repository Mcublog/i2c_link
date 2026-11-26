/**
 * @file slave_test.cpp
 * @brief Юнит-тесты slave-режима: сервис событий и команды настройки
 * @version 0.1
 * @date 2026-11-25
 *
 */
#include <cstdint>
#include <cstring>
#include <vector>

#include "app/i2c/i2c_service.h"
#include "app/i2c/i2c_slave_service.h"
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

/** @brief Перехваченный вывод транспорта */
static std::vector<uint8_t> m_out;
/** @brief Данные, переданные платформе для фазы чтения */
static std::vector<uint8_t> m_supplied;
/** @brief Сколько раз вызывалась передача данных мастеру */
static int m_supply_calls;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Колбэк передачи канального уровня
 *
 * @param data данные
 * @param size размер
 * @param ctx контекст
 * @return true - принято
 */
static bool capture_write(const uint8_t *data, uint32_t size, void *ctx)
{
    (void)ctx;
    m_out.insert(m_out.end(), data, data + size);
    return true;
}

/**
 * @brief Платформенный мок передачи данных мастеру
 *
 * Перекрывает weak-заглушку desktop-слоя.
 *
 * @param data данные
 * @param len длина
 * @return true - принято
 */
extern "C" bool i2c_slave_hw_supply(const uint8_t *data, uint16_t len)
{
    m_supply_calls++;
    m_supplied.assign(data, data + len);
    return true;
}

/**
 * @brief Собрать сырой кадр запроса
 *
 * @param cmd команда
 * @param msg_id номер сообщения
 * @param flags флаги
 * @param addr адрес
 * @param data payload
 * @param len длина payload
 * @return std::vector<uint8_t> сырой кадр
 */
static std::vector<uint8_t> make_request(uint8_t cmd, uint8_t msg_id,
                                         uint8_t flags, uint8_t addr,
                                         const uint8_t *data, uint32_t len)
{
    std::vector<uint8_t> raw(PROTO_REQ_HDR_SIZE + len + PROTO_CRC_SIZE);

    raw[0] = (uint8_t)((PROTO_VERSION << PROTO_VERSION_SHIFT) | cmd);
    raw[1] = msg_id;
    raw[2] = flags;
    raw[3] = addr;
    raw[4] = (uint8_t)len;
    raw[5] = 0;
    raw[6] = 0;
    if (len != 0)
        memcpy(raw.data() + PROTO_REQ_HDR_SIZE, data, len);

    uint16_t crc = proto_crc16(raw.data(), PROTO_REQ_HDR_SIZE + (uint32_t)len);
    raw[PROTO_REQ_HDR_SIZE + len] = (uint8_t)(crc & 0xFF);
    raw[PROTO_REQ_HDR_SIZE + len + 1] = (uint8_t)(crc >> 8);
    return raw;
}

/**
 * @brief Закодировать кадр в COBS/R с терминатором
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
 * @brief Разобрать кадр с провода (ответ или событие устройства)
 *
 * @param wire данные с провода
 * @param resp разобранный кадр
 * @param data payload
 * @return true - успешно
 */
static bool parse_frame(const std::vector<uint8_t> &wire,
                        proto_resp_t *resp, std::vector<uint8_t> *data)
{
    uint8_t raw[PROTO_MAX_RAW_FRAME];

    if (wire.empty() || (wire.back() != 0x00))
        return false;

    cobsr_decode_result decoded =
        cobsr_decode(raw, sizeof(raw), wire.data(), wire.size() - 1);
    if (decoded.status != COBSR_DECODE_OK)
        return false;

    uint32_t size = (uint32_t)decoded.out_len;
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
 * @brief Разобрать единственный кадр из накопленного вывода
 *
 * @param resp разобранный кадр
 * @param data payload
 * @return true - успешно
 */
static bool take_frame(proto_resp_t *resp, std::vector<uint8_t> *data)
{
    bool ok = parse_frame(m_out, resp, data);
    m_out.clear();
    return ok;
}

/**
 * @brief Подать запрос в канальный уровень
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
 * @brief Отправить очередь событий slave-сервиса в транспорт
 *
 * @param link канальный уровень
 */
static void flush_events(proto_link_t *link)
{
    i2c_slave_service_poll();
    proto_link_poll(link);
}

/**
 * @brief Подготовить канальный уровень и сервисы
 *
 * @param link канальный уровень
 */
static void setup(proto_link_t *link)
{
    m_out.clear();
    m_supplied.clear();
    m_supply_calls = 0;

    proto_link_init(link, capture_write, i2c_service_handle, link);
    i2c_service_init(link);
}

static void test_write_event(void)
{
    proto_link_t link;
    setup(&link);

    const uint8_t kData[] = {0x10, 0x20, 0x30};
    i2c_slave_service_on_write(0x42, kData, sizeof(kData), false);
    CHECK(i2c_slave_service_busy());
    flush_events(&link);
    CHECK(!i2c_slave_service_busy());

    proto_resp_t event{};
    std::vector<uint8_t> data;
    CHECK(take_frame(&event, &data));
    CHECK(event.cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK((event.flags & PROTO_EVENT_FLAG_WRITE) != 0);
    CHECK((event.flags & PROTO_EVENT_FLAG_READ) == 0);
    CHECK(event.addr == 0x42);
    CHECK(event.status == PROTO_STATUS_OK);
    CHECK(data == std::vector<uint8_t>(kData, kData + sizeof(kData)));
}

static void test_write_then_read(void)
{
    proto_link_t link;
    setup(&link);

    const uint8_t kData[] = {0x00, 0x55};
    i2c_slave_service_on_write(0x42, kData, sizeof(kData), true);
    flush_events(&link);

    proto_resp_t event{};
    std::vector<uint8_t> data;
    CHECK(take_frame(&event, &data));
    CHECK(event.cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK(event.flags == (PROTO_EVENT_FLAG_WRITE | PROTO_EVENT_FLAG_READ));
    CHECK(i2c_slave_service_busy());

    // Ответ хоста приходит кадром-запросом с тем же msg_id
    const uint8_t kRead[] = {0xAA, 0xBB, 0xCC};
    feed_request(&link,
                 make_request(PROTO_CMD_SLAVE_EVENT, event.msg_id, 0, 0x42,
                              kRead, sizeof(kRead)));
    m_out.clear();

    CHECK(m_supply_calls == 1);
    CHECK(m_supplied == std::vector<uint8_t>(kRead, kRead + sizeof(kRead)));
    CHECK(!i2c_slave_service_busy());
}

static void test_read_event(void)
{
    proto_link_t link;
    setup(&link);

    i2c_slave_service_on_read(0x42);
    flush_events(&link);

    proto_resp_t event{};
    std::vector<uint8_t> data;
    CHECK(take_frame(&event, &data));
    CHECK(event.cmd == PROTO_CMD_SLAVE_EVENT);
    CHECK(event.flags == PROTO_EVENT_FLAG_READ);
    CHECK(event.len == 0);
    CHECK(event.addr == 0x42);

    const uint8_t kRead[] = {0x01};
    feed_request(&link,
                 make_request(PROTO_CMD_SLAVE_EVENT, event.msg_id, 0, 0x42,
                              kRead, sizeof(kRead)));
    m_out.clear();

    CHECK(m_supply_calls == 1);
    CHECK(m_supplied == std::vector<uint8_t>(kRead, kRead + sizeof(kRead)));
}

static void test_stale_reply_ignored(void)
{
    proto_link_t link;
    setup(&link);

    i2c_slave_service_on_read(0x42);
    flush_events(&link);

    proto_resp_t event{};
    std::vector<uint8_t> data;
    CHECK(take_frame(&event, &data));

    uint8_t wrong = (uint8_t)((event.msg_id + 1) & PROTO_MSG_ID_MASK);
    const uint8_t kRead[] = {0x99};
    feed_request(&link,
                 make_request(PROTO_CMD_SLAVE_EVENT, wrong, 0, 0x42, kRead,
                              sizeof(kRead)));
    m_out.clear();

    CHECK(m_supply_calls == 0);
    CHECK(i2c_slave_service_busy());
}

static void test_done_event(void)
{
    proto_link_t link;
    setup(&link);

    i2c_slave_service_on_done(0x42, 5, 2, PROTO_STATUS_OK);
    flush_events(&link);

    proto_resp_t event{};
    std::vector<uint8_t> data;
    CHECK(take_frame(&event, &data));
    CHECK(event.cmd == PROTO_CMD_SLAVE_DONE);
    CHECK(event.flags == (PROTO_EVENT_FLAG_WRITE | PROTO_EVENT_FLAG_READ));
    CHECK(event.len == PROTO_SLAVE_DONE_SIZE);
    CHECK(data.size() == PROTO_SLAVE_DONE_SIZE);
    CHECK(data[0] == 5);
    CHECK(data[1] == 2);
}

static void test_overflow_write(void)
{
    proto_link_t link;
    setup(&link);

    std::vector<uint8_t> big(PROTO_MAX_DATA + 10, 0x5A);
    i2c_slave_service_on_write(0x42, big.data(), (uint16_t)big.size(), false);
    flush_events(&link);

    proto_resp_t event{};
    std::vector<uint8_t> data;
    CHECK(take_frame(&event, &data));
    CHECK(event.status == PROTO_STATUS_OVERFLOW);
    CHECK(event.len == PROTO_MAX_DATA);
}

static void test_config_commands(void)
{
    proto_link_t link;
    setup(&link);

    proto_resp_t resp{};
    std::vector<uint8_t> data;

    // Адрес по умолчанию
    feed_request(&link,
                 make_request(PROTO_CMD_GET_CONFIG, 0x01, 0, 0, nullptr, 0));
    CHECK(take_frame(&resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);
    CHECK(data.size() == PROTO_CONFIG_SIZE);
    CHECK(data[0] == PROTO_MODE_MASTER);

    // Установка корректного адреса
    const uint8_t kAddr = 0x50;
    feed_request(&link, make_request(PROTO_CMD_SET_SLAVE_ADDR, 0x02, 0, 0,
                                     &kAddr, 1));
    CHECK(take_frame(&resp, &data));
    CHECK(resp.status == PROTO_STATUS_OK);

    feed_request(&link,
                 make_request(PROTO_CMD_GET_CONFIG, 0x03, 0, 0, nullptr, 0));
    CHECK(take_frame(&resp, &data));
    CHECK(data.size() == PROTO_CONFIG_SIZE);
    CHECK(data[1] == kAddr);

    // Недопустимый адрес
    const uint8_t kBadAddr = 0x01;
    feed_request(&link, make_request(PROTO_CMD_SET_SLAVE_ADDR, 0x04, 0, 0,
                                     &kBadAddr, 1));
    CHECK(take_frame(&resp, &data));
    CHECK(resp.status == PROTO_STATUS_PARAM);

    // Slave-режим на эмуляторе не поддерживается
    const uint8_t kSlaveMode = PROTO_MODE_SLAVE;
    feed_request(&link, make_request(PROTO_CMD_SET_MODE, 0x05, 0, 0,
                                     &kSlaveMode, 1));
    CHECK(take_frame(&resp, &data));
    CHECK(resp.status == PROTO_STATUS_NOT_SUPPORTED);

    // Недопустимый режим
    const uint8_t kBadMode = 0x07;
    feed_request(&link,
                 make_request(PROTO_CMD_SET_MODE, 0x06, 0, 0, &kBadMode, 1));
    CHECK(take_frame(&resp, &data));
    CHECK(resp.status == PROTO_STATUS_PARAM);
}
//<<----------------------

//>>---------------------- Public
int main()
{
    test_write_event();
    test_write_then_read();
    test_read_event();
    test_stale_reply_ignored();
    test_done_event();
    test_overflow_write();
    test_config_commands();

    if (m_failures != 0)
    {
        LOG_ERROR("slave tests failed: %d checks", m_failures);
        return -1;
    }

    LOG_INFO("slave tests passed");
    return 0;
}
//<<----------------------