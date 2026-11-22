/**
 * @file usb_vcp.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief USB VCP (CDC) транспорт для STM32F072
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "hw/usb_vcp.h"

#include "main.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME usbvcp
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------
//>>---------------------- Local
#ifdef HAL_PCD_MODULE_ENABLED
#include "usbd_cdc_if.h"
static ios_irq_handler_t m_irq_handler = NULL;
#endif
//<<----------------------

//>>---------------------- Public
bool usb_vcp_init(ios_irq_handler_t handler)
{
#ifdef HAL_PCD_MODULE_ENABLED
    m_irq_handler = handler;
    return handler != NULL;
#else
    (void)handler;
    LOG_WARNING("USB device is not enabled in CubeMX");
    return false;
#endif
}

bool usb_vcp_write(const uint8_t *data, uint32_t size)
{
#ifdef HAL_PCD_MODULE_ENABLED
    if (!data || size == 0 || size > UINT16_MAX)
        return false;
    return CDC_Transmit_FS((uint8_t *)data, (uint16_t)size) == USBD_OK;
#else
    (void)data;
    (void)size;
    return false;
#endif
}

void usb_vcp_irq_handler(ios_chunk_t *chunk)
{
#ifdef HAL_PCD_MODULE_ENABLED
    if (m_irq_handler)
        m_irq_handler(chunk);
#else
    (void)chunk;
#endif
}
//<<----------------------