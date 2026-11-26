/**
 * @file i2c_service.c
 * @brief Обработчики команд протокола host <-> I2C
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include "app/i2c/i2c_service.h"

#include <string.h>

#include "app/i2c/i2c_slave_service.h"
#include "app/i2c/ii2c.h"
#include "app/i2c/islave.h"
#include "app/io/activity.h"
#include "app/proto/link.h"
#include "app/version.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME i2csvc
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Адрес регистра по умолчанию: начало диапазона сканирования */
#define DEFAULT_SCAN_FROM (0x08)
/** @brief Адрес регистра по умолчанию: конец диапазона сканирования */
#define DEFAULT_SCAN_TO (0x77)
/** @brief Адрес slave по умолчанию */
#define DEFAULT_SLAVE_ADDR (0x42)
/** @brief Битовая маска поддерживаемых команд для INFO */
#define SUPPORTED_CAPS                                                        \
    ((uint8_t)((1u << PROTO_CMD_READ) | (1u << PROTO_CMD_WRITE) |            \
               (1u << PROTO_CMD_SCAN) | (1u << PROTO_CMD_INFO) |             \
               (1u << PROTO_CMD_PING) | (1u << PROTO_CMD_BUS_RECOVER) |      \
               (1u << PROTO_CMD_STATS)))

/** @brief Буфер данных ответа */
static uint8_t m_data[PROTO_MAX_DATA];
/** @brief Счётчик NACK */
static uint16_t m_nack;
/** @brief Счётчик таймаутов */
static uint16_t m_timeout;
/** @brief Текущий режим работы шины */
static uint8_t m_mode;
/** @brief Настроенный 7-битный адрес slave */
static uint8_t m_slave_addr;
//<<----------------------

_Static_assert(PROTO_SCAN_BITMAP_SIZE == I2C_SCAN_BITMAP_SIZE,
               "scan bitmap size mismatch");

//>>---------------------- Private
/**
 * @brief Перевести статус шины в статус протокола
 *
 * @param status статус шины
 * @return proto_status_t статус протокола
 */
static proto_status_t map_status(i2c_status_t status);

/**
 * @brief Обработать READ
 *
 * @param req запрос
 * @param resp ответ
 */
static void handle_read(const proto_req_t *req, proto_resp_t *resp);

/**
 * @brief Обработать WRITE
 *
 * @param req запрос
 * @param resp ответ
 */
static void handle_write(const proto_req_t *req, proto_resp_t *resp);

/**
 * @brief Обработать SCAN
 *
 * @param req запрос
 * @param resp ответ
 */
static void handle_scan(const proto_req_t *req, proto_resp_t *resp);

/**
 * @brief Обработать INFO
 *
 * @param resp ответ
 */
static void handle_info(proto_resp_t *resp);

/**
 * @brief Обработать PING
 *
 * @param req запрос
 * @param resp ответ
 */
static void handle_ping(const proto_req_t *req, proto_resp_t *resp);

/**
 * @brief Обработать BUS_RECOVER
 *
 * @param resp ответ
 */
static void handle_bus_recover(proto_resp_t *resp);

/**
 * @brief Обработать STATS
 *
 * @param resp ответ
 * @param ctx контекст proto_link_t
 */
static void handle_stats(proto_resp_t *resp, void *ctx);

/**
 * @brief Обработать SET_MODE
 *
 * @param req запрос
 * @param resp ответ
 */
static void handle_set_mode(const proto_req_t *req, proto_resp_t *resp);

/**
 * @brief Обработать SET_SLAVE_ADDR
 *
 * @param req запрос
 * @param resp ответ
 */
static void handle_set_slave_addr(const proto_req_t *req,
                                  proto_resp_t *resp);

/**
 * @brief Обработать GET_CONFIG
 *
 * @param resp ответ
 */
static void handle_get_config(proto_resp_t *resp);

/**
 * @brief Обработать ответ хоста на событие slave-режима
 *
 * @param req запрос
 * @param resp ответ
 */
static void handle_slave_event(const proto_req_t *req, proto_resp_t *resp);

/**
 * @brief Разобрать строку версии прошивки
 *
 * @param out результат: major, minor, patch
 */
static void parse_fw_version(uint8_t out[3]);
//<<----------------------

//>>---------------------- Private
static proto_status_t map_status(i2c_status_t status)
{
    switch (status)
    {
    case I2C_STATUS_OK:
        return PROTO_STATUS_OK;
    case I2C_STATUS_NACK:
        m_nack++;
        return PROTO_STATUS_NACK_ADDR;
    case I2C_STATUS_TIMEOUT:
        m_timeout++;
        return PROTO_STATUS_TIMEOUT;
    case I2C_STATUS_BUS:
        return PROTO_STATUS_BUS_ERROR;
    case I2C_STATUS_PARAM:
        return PROTO_STATUS_PARAM;
    case I2C_STATUS_BUSY:
        return PROTO_STATUS_BUSY;
    case I2C_STATUS_NOT_SUPPORTED:
    default:
        return PROTO_STATUS_NOT_SUPPORTED;
    }
}

static void handle_read(const proto_req_t *req, proto_resp_t *resp)
{
    uint8_t reg_size = req->flags & PROTO_REG_SIZE_MASK;

    activity_begin();
    i2c_status_t status =
        i2c_read(req->addr, req->reg, reg_size, m_data, req->len);
    activity_end();

    resp->status = map_status(status);
    if (status != I2C_STATUS_OK)
        return;

    resp->len = req->len;
    resp->data = (req->len != 0) ? m_data : NULL;
}

static void handle_write(const proto_req_t *req, proto_resp_t *resp)
{
    uint8_t reg_size = req->flags & PROTO_REG_SIZE_MASK;

    activity_begin();
    i2c_status_t status =
        i2c_write(req->addr, req->reg, reg_size, req->data, req->len);
    activity_end();

    resp->status = map_status(status);
}

static void handle_scan(const proto_req_t *req, proto_resp_t *resp)
{
    uint8_t from = DEFAULT_SCAN_FROM;
    uint8_t to = DEFAULT_SCAN_TO;

    if (req->len == 2)
    {
        from = req->data[0];
        to = req->data[1];
    }

    activity_begin();
    i2c_status_t status = i2c_scan(from, to, m_data);
    activity_end();

    resp->status = map_status(status);
    if (status != I2C_STATUS_OK)
        return;

    resp->len = PROTO_SCAN_BITMAP_SIZE;
    resp->data = m_data;
}

static void handle_info(proto_resp_t *resp)
{
    uint8_t version[3] = {0};
    uint32_t speed = i2c_speed_khz();

    parse_fw_version(version);

    m_data[0] = PROTO_VERSION;
    m_data[1] = SUPPORTED_CAPS;
    m_data[2] = PROTO_MAX_DATA;
    m_data[3] = version[0];
    m_data[4] = version[1];
    m_data[5] = version[2];
    m_data[6] = (uint8_t)(speed & 0xFF);
    m_data[7] = (uint8_t)((speed >> 8) & 0xFF);

    resp->len = PROTO_INFO_SIZE;
    resp->data = m_data;
}

static void handle_ping(const proto_req_t *req, proto_resp_t *resp)
{
    if (req->len != 0)
        memcpy(m_data, req->data, req->len);

    resp->len = req->len;
    resp->data = (req->len != 0) ? m_data : NULL;
}

static void handle_bus_recover(proto_resp_t *resp)
{
    activity_begin();
    i2c_status_t status = i2c_recover();
    activity_end();

    resp->status = map_status(status);
}

static void handle_stats(proto_resp_t *resp, void *ctx)
{
    const proto_stats_t *stats =
        proto_link_stats((const proto_link_t *)ctx);

    if (stats == NULL)
    {
        resp->status = PROTO_STATUS_NOT_SUPPORTED;
        return;
    }

    uint16_t counters[7] = {
        stats->rx_ok,   stats->rx_crc_err, stats->rx_overflow,
        stats->rx_frame_err, stats->tx_drop, m_nack,
        m_timeout,
    };

    for (uint32_t i = 0; i < 7; ++i)
    {
        m_data[i * 2] = (uint8_t)(counters[i] & 0xFF);
        m_data[i * 2 + 1] = (uint8_t)((counters[i] >> 8) & 0xFF);
    }

    resp->len = PROTO_STATS_SIZE;
    resp->data = m_data;
}

static void handle_set_mode(const proto_req_t *req, proto_resp_t *resp)
{
    uint8_t mode = req->data[0];

    if (mode == PROTO_MODE_MASTER)
    {
        i2c_slave_hw_stop();
        i2c_slave_service_reset();
        if (!i2c_init())
        {
            resp->status = PROTO_STATUS_BUS_ERROR;
            return;
        }
        m_mode = mode;
        resp->status = PROTO_STATUS_OK;
        return;
    }

    if (mode == PROTO_MODE_SLAVE)
    {
        if (!i2c_slave_hw_start(m_slave_addr))
        {
            resp->status = PROTO_STATUS_NOT_SUPPORTED;
            return;
        }
        i2c_slave_service_reset();
        m_mode = mode;
        resp->status = PROTO_STATUS_OK;
        return;
    }

    resp->status = PROTO_STATUS_PARAM;
}

static void handle_set_slave_addr(const proto_req_t *req, proto_resp_t *resp)
{
    uint8_t addr = req->data[0];

    if ((addr < I2C_SLAVE_ADDR_MIN) || (addr > I2C_SLAVE_ADDR_MAX))
    {
        resp->status = PROTO_STATUS_PARAM;
        return;
    }

    if (m_mode == PROTO_MODE_SLAVE)
    {
        i2c_slave_hw_stop();
        if (!i2c_slave_hw_start(addr))
        {
            i2c_slave_hw_start(m_slave_addr);
            resp->status = PROTO_STATUS_NOT_SUPPORTED;
            return;
        }
        i2c_slave_service_reset();
    }

    m_slave_addr = addr;
    resp->status = PROTO_STATUS_OK;
}

static void handle_get_config(proto_resp_t *resp)
{
    m_data[0] = m_mode;
    m_data[1] = m_slave_addr;

    resp->len = PROTO_CONFIG_SIZE;
    resp->data = m_data;
}

static void handle_slave_event(const proto_req_t *req, proto_resp_t *resp)
{
    i2c_slave_service_on_reply(req);
    resp->status = PROTO_STATUS_OK;
}

static void parse_fw_version(uint8_t out[3])
{
    const char *text = FW_VERSION;
    uint32_t part = 0;
    uint32_t index = 0;

    out[0] = 0;
    out[1] = 0;
    out[2] = 0;

    for (; (*text != '\0') && (index < 3); ++text)
    {
        char symbol = *text;

        if ((symbol >= '0') && (symbol <= '9'))
        {
            part = (part * 10) + (uint32_t)(symbol - '0');
            if (part > 0xFF)
                part = 0xFF;
            continue;
        }
        if (symbol != '.')
            break;

        out[index++] = (uint8_t)part;
        part = 0;
    }

    if (index < 3)
        out[index] = (uint8_t)part;
}
//<<----------------------

//>>---------------------- Public
void i2c_service_init(proto_link_t *link)
{
    m_nack = 0;
    m_timeout = 0;
    m_mode = PROTO_MODE_MASTER;
    m_slave_addr = DEFAULT_SLAVE_ADDR;

    i2c_slave_service_init(link);

    if (!i2c_init())
        LOG_ERROR("i2c init failed");
}

void i2c_service_poll(void)
{
    i2c_slave_service_poll();
}

void i2c_service_handle(const proto_req_t *req, proto_resp_t *resp, void *ctx)
{
    if ((req == NULL) || (resp == NULL))
        return;

    switch (req->cmd)
    {
    case PROTO_CMD_READ:
        handle_read(req, resp);
        break;
    case PROTO_CMD_WRITE:
        handle_write(req, resp);
        break;
    case PROTO_CMD_SCAN:
        handle_scan(req, resp);
        break;
    case PROTO_CMD_INFO:
        handle_info(resp);
        break;
    case PROTO_CMD_PING:
        handle_ping(req, resp);
        break;
    case PROTO_CMD_BUS_RECOVER:
        handle_bus_recover(resp);
        break;
    case PROTO_CMD_STATS:
        handle_stats(resp, ctx);
        break;
    case PROTO_CMD_SET_MODE:
        handle_set_mode(req, resp);
        break;
    case PROTO_CMD_SET_SLAVE_ADDR:
        handle_set_slave_addr(req, resp);
        break;
    case PROTO_CMD_GET_CONFIG:
        handle_get_config(resp);
        break;
    case PROTO_CMD_SLAVE_EVENT:
        handle_slave_event(req, resp);
        break;
    default:
        resp->status = PROTO_STATUS_NOT_SUPPORTED;
        break;
    }
}
//<<----------------------