/**
 * @file tiny_usb_port.c
 * @brief Обработка vendor-запросов EP0 для моста i2c-tiny-usb
 * @version 0.1
 * @date 2026-11-25
 *
 * Vendor-интерфейс не имеет эндпоинтов: весь обмен идёт через EP0
 * управляющими транзакциями (протокол i2c-tiny-usb). Транзакции шины
 * I2C выполняются синхронно в контексте tud_task(), то есть в главном
 * цикле, а не в прерывании USB.
 */
#include "tusb.h"

#include "app/bridge/tiny_usb.h"
#include "usb/tiny_usb_port.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME tinyusb
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Состояние моста */
static tiny_usb_t m_ctx;
/** @brief Буфер данных одной транзакции */
static uint8_t m_buf[TINY_USB_BUF_SIZE];

/**
 * @brief Обработать stage SETUP control-трансфера
 *
 * @param rhport номер порта
 * @param request запрос
 * @return true - запрос обработан
 */
static bool handle_setup(uint8_t rhport,
                         tusb_control_request_t const *request)
{
    tiny_usb_req_t req;
    req.type = request->bmRequestType;
    req.request = request->bRequest;
    req.value = request->wValue;
    req.index = request->wIndex;
    req.length = request->wLength;

    uint16_t len = 0;
    tiny_usb_result_t result = tiny_usb_setup(&m_ctx, &req, m_buf, &len);

    switch (result)
    {
    case TINY_USB_RESULT_IMMEDIATE:
        if (request->wLength == 0)
            return tud_control_status(rhport, request);
        return tud_control_xfer(rhport, request, m_buf, len);

    case TINY_USB_RESULT_ARM_OUT:
        // NOTE: данные записи принимаются в m_buf, транзакция I2C
        // выполняется на stage DATA
        return tud_control_xfer(rhport, request, m_buf, len);

    default:
        return false;
    }
}

/**
 * @brief Обработать stage DATA control-трансфера
 *
 * @param request запрос
 * @return true - обработка успешна
 */
static bool handle_data(tusb_control_request_t const *request)
{
    if (request->bmRequestType_bit.direction == TUSB_DIR_OUT)
        tiny_usb_write(&m_ctx, m_buf, request->wLength);

    return true;
}
//<<----------------------

//>>---------------------- Public
void tiny_usb_port_init(void)
{
    tiny_usb_init(&m_ctx);
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                tusb_control_request_t const *request)
{
    switch (stage)
    {
    case CONTROL_STAGE_SETUP:
        return handle_setup(rhport, request);

    case CONTROL_STAGE_DATA:
        return handle_data(request);

    default:
        return true;
    }
}
//<<----------------------
