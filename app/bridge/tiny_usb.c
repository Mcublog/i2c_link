/**
 * @file tiny_usb.c
 * @brief Мост i2c-tiny-usb поверх интерфейса шины I2C
 * @version 0.1
 * @date 2026-11-25
 *
 */
#include "app/bridge/tiny_usb.h"

#include <string.h>

#include "app/i2c/ii2c.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME tinyusb
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Private
/**
 * @brief Перевести статус шины в статус моста
 *
 * Драйвер i2c-tiny-usb различает только ACK и NACK адреса, поэтому
 * любая ошибка транзакции отображается в NACK.
 *
 * @param status статус шины
 * @return uint8_t статус моста
 */
static uint8_t map_status(i2c_status_t status)
{
    return (status == I2C_STATUS_OK) ? TINY_USB_STATUS_ACK
                                     : TINY_USB_STATUS_NAK;
}

/**
 * @brief Выполнить чтение из I2C
 *
 * @param ctx состояние
 * @param addr 7-битный адрес
 * @param data буфер
 * @param len длина
 */
static void read_from_bus(tiny_usb_t *ctx, uint8_t addr, uint8_t *data,
                          uint16_t len)
{
    i2c_status_t status = i2c_read(addr, 0, 0, data, len);

    ctx->status = map_status(status);
    if (status != I2C_STATUS_OK)
    {
        // NOTE: драйвер требует ровно wLength байт, иначе сообщает
        // ошибку ввода-вывода до чтения статуса; поэтому отдаём
        // буфер целиком, а NACK передаём через GET_STATUS
        memset(data, 0xFF, len);
    }
}
//<<----------------------

//>>---------------------- Public
void tiny_usb_init(tiny_usb_t *ctx)
{
    if (ctx == NULL)
        return;

    memset(ctx, 0, sizeof(*ctx));
    ctx->status = TINY_USB_STATUS_IDLE;
}

tiny_usb_result_t tiny_usb_setup(tiny_usb_t *ctx,
                                 const tiny_usb_req_t *req,
                                 uint8_t *data, uint16_t *len)
{
    if ((ctx == NULL) || (req == NULL) || (data == NULL) || (len == NULL))
        return TINY_USB_RESULT_STALL;

    *len = 0;

    // NOTE: флаги BEGIN/END приходят только в младших битах команды
    // I2C_IO (bRequest = 4..7); команды ECHO..GET_STATUS идут без флагов
    uint8_t cmd = (uint8_t)(req->request & 0xFCu);
    if (cmd != TINY_USB_CMD_IO)
        cmd = req->request;

    switch (cmd)
    {
    case TINY_USB_CMD_ECHO:
        return TINY_USB_RESULT_IMMEDIATE;

    case TINY_USB_CMD_GET_FUNC:
        if (req->length < sizeof(uint32_t))
            return TINY_USB_RESULT_STALL;
        data[0] = (uint8_t)(TINY_USB_FUNC_MASK & 0xFF);
        data[1] = (uint8_t)((TINY_USB_FUNC_MASK >> 8) & 0xFF);
        data[2] = (uint8_t)((TINY_USB_FUNC_MASK >> 16) & 0xFF);
        data[3] = (uint8_t)((TINY_USB_FUNC_MASK >> 24) & 0xFF);
        *len = sizeof(uint32_t);
        return TINY_USB_RESULT_IMMEDIATE;

    case TINY_USB_CMD_SET_DELAY:
        ctx->delay_us = req->value;
        return TINY_USB_RESULT_IMMEDIATE;

    case TINY_USB_CMD_GET_STATUS:
        if (req->length < 1)
            return TINY_USB_RESULT_STALL;
        data[0] = ctx->status;
        *len = 1;
        return TINY_USB_RESULT_IMMEDIATE;

    case TINY_USB_CMD_IO:
        break;

    default:
        return TINY_USB_RESULT_STALL;
    }

    // NOTE: флаги BEGIN/END в фазе 1 не используются: каждый сегмент
    // выполняется как отдельная транзакция START...STOP
    if (req->length > TINY_USB_BUF_SIZE)
        return TINY_USB_RESULT_STALL;

    uint8_t addr = (uint8_t)(req->index & 0x7F);
    bool is_read = (req->type & TINY_USB_DIR_IN) != 0;

    if (req->length == 0)
    {
        ctx->status = map_status(i2c_probe(addr));
        return TINY_USB_RESULT_IMMEDIATE;
    }

    if (is_read)
    {
        read_from_bus(ctx, addr, data, req->length);
        *len = req->length;
        return TINY_USB_RESULT_IMMEDIATE;
    }

    ctx->addr = addr;
    ctx->write_pending = true;
    *len = req->length;
    return TINY_USB_RESULT_ARM_OUT;
}

void tiny_usb_write(tiny_usb_t *ctx, const uint8_t *data, uint16_t len)
{
    if ((ctx == NULL) || (data == NULL) || !ctx->write_pending)
        return;

    ctx->write_pending = false;
    ctx->status = map_status(i2c_write(ctx->addr, 0, 0, data, len));
}
//<<----------------------
