/**
 * @file tiny_usb_test.cpp
 * @brief Юнит-тесты моста i2c-tiny-usb
 * @version 0.1
 * @date 2026-11-25
 *
 */
#include <cstdint>
#include <cstring>

#include "app/bridge/tiny_usb.h"
#include "app/i2c/ii2c.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME utest
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Счётчик неудачных проверок */
static int m_failures;

/** @brief Адрес присутствующего устройства в эмуляторе шины */
#define DEV_ADDR (0x0C)
/** @brief Адрес отсутствующего устройства */
#define NO_DEV_ADDR (0x33)

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

/**
 * @brief Сформировать запрос
 *
 * @param type bmRequestType
 * @param request bRequest
 * @param value wValue
 * @param index wIndex
 * @param length wLength
 * @return tiny_usb_req_t запрос
 */
static tiny_usb_req_t make_req(uint8_t type, uint8_t request,
                               uint16_t value, uint16_t index,
                               uint16_t length)
{
    tiny_usb_req_t req;
    req.type = type;
    req.request = request;
    req.value = value;
    req.index = index;
    req.length = length;
    return req;
}

/**
 * @brief Проверить маску функций I2C
 *
 * @param ctx состояние моста
 */
static void test_get_func(tiny_usb_t *ctx)
{
    uint8_t buf[TINY_USB_BUF_SIZE];
    uint16_t len = 0;
    tiny_usb_req_t req = make_req(0xC0, TINY_USB_CMD_GET_FUNC, 0, 0, 4);

    tiny_usb_result_t result = tiny_usb_setup(ctx, &req, buf, &len);
    CHECK(result == TINY_USB_RESULT_IMMEDIATE);
    CHECK(len == 4);

    uint32_t mask = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                    ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    CHECK(mask == TINY_USB_FUNC_MASK);

    req.length = 2;
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) == TINY_USB_RESULT_STALL);
}

/**
 * @brief Проверить команду установки задержки
 *
 * @param ctx состояние моста
 */
static void test_set_delay(tiny_usb_t *ctx)
{
    uint8_t buf[TINY_USB_BUF_SIZE];
    uint16_t len = 0;
    tiny_usb_req_t req = make_req(0x42, TINY_USB_CMD_SET_DELAY, 20, 0, 0);

    tiny_usb_result_t result = tiny_usb_setup(ctx, &req, buf, &len);
    CHECK(result == TINY_USB_RESULT_IMMEDIATE);
    CHECK(len == 0);
    CHECK(ctx->delay_us == 20);
}

/**
 * @brief Проверить чтение статуса
 *
 * @param ctx состояние моста
 * @param expected ожидаемый статус
 */
static void check_status(tiny_usb_t *ctx, uint8_t expected)
{
    uint8_t buf[TINY_USB_BUF_SIZE];
    uint16_t len = 0;
    tiny_usb_req_t req = make_req(0xC0, TINY_USB_CMD_GET_STATUS, 0, 0, 1);

    tiny_usb_result_t result = tiny_usb_setup(ctx, &req, buf, &len);
    CHECK(result == TINY_USB_RESULT_IMMEDIATE);
    CHECK(len == 1);
    CHECK(buf[0] == expected);
}

/**
 * @brief Проверить запись и последующее чтение
 *
 * @param ctx состояние моста
 */
static void test_write_read(tiny_usb_t *ctx)
{
    uint8_t buf[TINY_USB_BUF_SIZE];
    uint16_t len = 0;
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

    // запись: сначала принимаем данные, затем выполняем транзакцию
    tiny_usb_req_t req = make_req(0x42, TINY_USB_CMD_IO, 0, DEV_ADDR,
                                  sizeof(payload));
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) ==
          TINY_USB_RESULT_ARM_OUT);
    CHECK(len == sizeof(payload));

    memcpy(buf, payload, sizeof(payload));
    tiny_usb_write(ctx, buf, sizeof(payload));
    check_status(ctx, TINY_USB_STATUS_ACK);

    // чтение с флагами BEGIN/END: они не должны мешать разбору
    req = make_req(0xC2, (uint8_t)(TINY_USB_CMD_IO | TINY_USB_IO_BEGIN |
                                   TINY_USB_IO_END),
                   0, DEV_ADDR, sizeof(payload));
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) ==
          TINY_USB_RESULT_IMMEDIATE);
    CHECK(len == sizeof(payload));
    CHECK(memcmp(buf, payload, sizeof(payload)) == 0);
    check_status(ctx, TINY_USB_STATUS_ACK);
}

/**
 * @brief Проверить чтение из отсутствующего устройства
 *
 * @param ctx состояние моста
 */
static void test_read_nack(tiny_usb_t *ctx)
{
    uint8_t buf[TINY_USB_BUF_SIZE];
    uint16_t len = 0;
    tiny_usb_req_t req =
        make_req(0xC2, TINY_USB_CMD_IO, 0, NO_DEV_ADDR, 8);

    CHECK(tiny_usb_setup(ctx, &req, buf, &len) ==
          TINY_USB_RESULT_IMMEDIATE);
    CHECK(len == 8);

    for (uint16_t i = 0; i < len; i++)
        CHECK(buf[i] == 0xFF);

    check_status(ctx, TINY_USB_STATUS_NAK);
}

/**
 * @brief Проверить пробу адреса (транзакция нулевой длины)
 *
 * @param ctx состояние моста
 */
static void test_probe(tiny_usb_t *ctx)
{
    uint8_t buf[TINY_USB_BUF_SIZE];
    uint16_t len = 0;

    tiny_usb_req_t req = make_req(0x42, TINY_USB_CMD_IO, 0, DEV_ADDR, 0);
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) ==
          TINY_USB_RESULT_IMMEDIATE);
    CHECK(len == 0);
    check_status(ctx, TINY_USB_STATUS_ACK);

    req = make_req(0xC2, TINY_USB_CMD_IO, 0, NO_DEV_ADDR, 0);
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) ==
          TINY_USB_RESULT_IMMEDIATE);
    CHECK(len == 0);
    check_status(ctx, TINY_USB_STATUS_NAK);
}

/**
 * @brief Проверить отказ для неизвестных и неверных запросов
 *
 * @param ctx состояние моста
 */
static void test_invalid(tiny_usb_t *ctx)
{
    uint8_t buf[TINY_USB_BUF_SIZE];
    uint16_t len = 0;

    tiny_usb_req_t req = make_req(0x42, 0x09, 0, 0, 0);
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) == TINY_USB_RESULT_STALL);

    req = make_req(0xC2, TINY_USB_CMD_IO, 0, DEV_ADDR,
                   TINY_USB_BUF_SIZE + 1);
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) == TINY_USB_RESULT_STALL);

    req = make_req(0xC0, TINY_USB_CMD_GET_STATUS, 0, 0, 0);
    CHECK(tiny_usb_setup(ctx, &req, buf, &len) == TINY_USB_RESULT_STALL);
}
//<<----------------------

//>>---------------------- Public
int main(void)
{
    CHECK(i2c_init());

    tiny_usb_t ctx;
    tiny_usb_init(&ctx);

    check_status(&ctx, TINY_USB_STATUS_IDLE);

    test_get_func(&ctx);
    test_set_delay(&ctx);
    test_write_read(&ctx);
    test_read_nack(&ctx);
    test_probe(&ctx);
    test_invalid(&ctx);

    if (m_failures == 0)
    {
        LOG_INFO("tiny_usb: all checks passed");
        return 0;
    }

    LOG_ERROR("tiny_usb: %d check(s) failed", m_failures);
    return 1;
}
//<<----------------------
