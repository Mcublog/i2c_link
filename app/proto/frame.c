/**
 * @file frame.c
 * @brief Сборка и разбор кадров протокола host <-> I2C
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include "app/proto/frame.h"

#include <stdbool.h>
#include <string.h>

//>>---------------------- Local
/** @brief Начальное значение CRC-16/CCITT-FALSE */
#define CRC16_INIT (0xFFFF)
/** @brief Полином CRC-16/CCITT-FALSE */
#define CRC16_POLY (0x1021)
/** @brief Количество бит в байте */
#define BITS_PER_BYTE (8)
//<<----------------------

//>>---------------------- Private
/**
 * @brief Проверить, что команда известна
 *
 * @param cmd команда
 * @return true команда поддерживается
 */
static bool is_known_cmd(proto_cmd_t cmd);

/**
 * @brief Проверить корректность полей запроса
 *
 * @param raw сырой кадр
 * @param req разобранный запрос
 * @param size размер сырого кадра
 * @return proto_status_t статус проверки
 */
static proto_status_t validate_req(const uint8_t *raw, proto_req_t *req,
                                   uint32_t size);

/**
 * @brief Проверить корректность диапазона сканирования
 *
 * @param req разобранный запрос
 * @return true диапазон корректен
 */
static bool is_valid_scan_range(const proto_req_t *req);
//<<----------------------

//>>---------------------- Private
static bool is_known_cmd(proto_cmd_t cmd)
{
    switch (cmd)
    {
    case PROTO_CMD_READ:
    case PROTO_CMD_WRITE:
    case PROTO_CMD_SCAN:
    case PROTO_CMD_INFO:
    case PROTO_CMD_PING:
    case PROTO_CMD_BUS_RECOVER:
    case PROTO_CMD_STATS:
        return true;
    default:
        return false;
    }
}

static bool is_valid_scan_range(const proto_req_t *req)
{
    if (req->len == 0)
        return true;
    if (req->len != 2)
        return false;
    if (req->data == NULL)
        return false;
    if (req->data[0] > req->data[1])
        return false;
    if (req->data[1] > PROTO_MSG_ID_MASK)
        return false;
    return true;
}

static proto_status_t validate_req(const uint8_t *raw, proto_req_t *req,
                                   uint32_t size)
{
    uint8_t reg_size = req->flags & PROTO_REG_SIZE_MASK;
    uint32_t payload_len = 0;

    if (req->msg_id & PROTO_MSG_DIR_RESPONSE)
        return PROTO_STATUS_FRAME;
    if (req->flags & PROTO_FLAGS_RESERVED_MASK)
        return PROTO_STATUS_PARAM;
    if (reg_size > 2 || req->addr > PROTO_MSG_ID_MASK)
        return PROTO_STATUS_PARAM;
    if (!is_known_cmd(req->cmd))
        return PROTO_STATUS_UNKNOWN_CMD;

    payload_len = (req->cmd == PROTO_CMD_READ) ? 0 : req->len;
    if ((payload_len > PROTO_MAX_DATA) ||
        ((req->cmd == PROTO_CMD_READ) && (req->len > PROTO_MAX_DATA)))
        return PROTO_STATUS_PARAM;

    if ((req->cmd != PROTO_CMD_READ) && (req->cmd != PROTO_CMD_WRITE) &&
        (reg_size != 0))
        return PROTO_STATUS_PARAM;

    if ((req->cmd == PROTO_CMD_INFO) || (req->cmd == PROTO_CMD_STATS) ||
        (req->cmd == PROTO_CMD_BUS_RECOVER))
    {
        if (req->len != 0)
            return PROTO_STATUS_PARAM;
    }

    if (size != PROTO_REQ_HDR_SIZE + payload_len + PROTO_CRC_SIZE)
        return PROTO_STATUS_PARAM;

    // NOTE: указатель на payload выставляется только после проверки размера
    req->data = (payload_len != 0) ? (raw + PROTO_REQ_HDR_SIZE) : NULL;

    if (req->cmd == PROTO_CMD_SCAN && !is_valid_scan_range(req))
        return PROTO_STATUS_PARAM;

    return PROTO_STATUS_OK;
}
//<<----------------------

//>>---------------------- Public
uint16_t proto_crc16(const uint8_t *data, uint32_t size)
{
    uint16_t crc = CRC16_INIT;

    if (data == NULL)
        return crc;

    for (uint32_t i = 0; i < size; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < BITS_PER_BYTE; ++bit)
        {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ CRC16_POLY);
            else
                crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

proto_status_t proto_frame_parse(const uint8_t *raw, uint32_t size,
                                 proto_req_t *req)
{
    if ((raw == NULL) || (req == NULL))
        return PROTO_STATUS_FRAME;
    if (size < PROTO_REQ_HDR_SIZE + PROTO_CRC_SIZE)
        return PROTO_STATUS_FRAME;

    uint8_t version = raw[0] & PROTO_VERSION_MASK;
    if (version != (uint8_t)(PROTO_VERSION << PROTO_VERSION_SHIFT))
        return PROTO_STATUS_FRAME;

    uint16_t crc = proto_crc16(raw, size - PROTO_CRC_SIZE);
    uint16_t crc_frame =
        (uint16_t)(raw[size - 2] | ((uint16_t)raw[size - 1] << 8));
    if (crc != crc_frame)
        return PROTO_STATUS_CRC;

    req->cmd = (proto_cmd_t)(raw[0] & PROTO_CMD_MASK);
    req->msg_id = raw[1];
    req->flags = raw[2];
    req->addr = raw[3];
    req->len = raw[4];
    req->reg = (uint16_t)(raw[5] | ((uint16_t)raw[6] << 8));
    req->data = NULL;

    return validate_req(raw, req, size);
}

uint32_t proto_frame_build_resp(uint8_t *buf, uint32_t cap,
                                const proto_resp_t *resp)
{
    if ((buf == NULL) || (resp == NULL))
        return 0;
    if (resp->len > PROTO_MAX_DATA)
        return 0;

    uint32_t total = PROTO_RESP_HDR_SIZE + resp->len + PROTO_CRC_SIZE;
    if (total > cap)
        return 0;
    if ((resp->len != 0) && (resp->data == NULL))
        return 0;

    buf[0] = (uint8_t)((PROTO_VERSION << PROTO_VERSION_SHIFT) |
                       (resp->cmd & PROTO_CMD_MASK));
    buf[1] = (uint8_t)(resp->msg_id | PROTO_MSG_DIR_RESPONSE);
    buf[2] = resp->flags;
    buf[3] = resp->addr;
    buf[4] = resp->len;
    buf[5] = (uint8_t)resp->status;

    if (resp->len != 0)
        memcpy(&buf[PROTO_RESP_HDR_SIZE], resp->data, resp->len);

    uint16_t crc =
        proto_crc16(buf, PROTO_RESP_HDR_SIZE + (uint32_t)resp->len);
    buf[PROTO_RESP_HDR_SIZE + resp->len] = (uint8_t)(crc & 0xFF);
    buf[PROTO_RESP_HDR_SIZE + resp->len + 1] = (uint8_t)(crc >> 8);

    return total;
}
//<<----------------------