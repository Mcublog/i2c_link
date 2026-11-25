/**
 * @file transport.c
 * @brief USB-транспорт (CDC) для платы fntester_pcb на стеке TinyUSB
 * @version 0.2
 * @date 2026-11-25
 *
 */
#include "app/io/itransport.h"

#include <string.h>

#include "tusb.h"
#include "usb/usb_app.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME trans
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Размер порции чтения из CDC */
#define TRANSPORT_RX_CHUNK (64)

/** @brief Обработчик приёма приложения */
static transport_rx_handler_t m_handler = NULL;
//<<----------------------

//>>---------------------- Public
bool transport_init(transport_rx_handler_t handler)
{
    m_handler = handler;
    return usb_app_init();
}

bool transport_write(const uint8_t *data, uint32_t size)
{
    if ((data == NULL) || (size == 0))
        return false;
    // NOTE: DTR не проверяем: данные копятся в FIFO CDC и уходят, как
    // только хост начнёт читать порт
    if (tud_cdc_write_available() < size)
        return false;
    if (tud_cdc_write(data, size) != size)
        return false;

    tud_cdc_write_flush();
    return true;
}

void transport_poll(void)
{
    usb_app_poll();
}

void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;

    uint8_t chunk[TRANSPORT_RX_CHUNK];

    for (;;)
    {
        uint32_t size = tud_cdc_read(chunk, sizeof(chunk));
        if (size == 0)
            break;
        if (m_handler != NULL)
            m_handler(chunk, size);
    }
}
//<<----------------------
