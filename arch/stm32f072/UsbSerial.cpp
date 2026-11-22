/**
 * @file UsbSerial.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Реализация USB VCP транспорта
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "UsbSerial.hpp"

#include "hw/usb_vcp.h"

//>>---------------------- Public
bool UsbSerial::init(ios_ctl_t *ctl)
{
    if (!ctl)
        return false;
    m_irq_handler = ctl->irq_handler;
    return usb_vcp_init(m_irq_handler);
}

bool UsbSerial::init(const char *portname, ios_ctl_t *ctl)
{
    (void)portname;
    return init(ctl);
}

bool UsbSerial::write(const uint8_t *data, uint32_t size)
{
    return usb_vcp_write(data, size);
}

uint8_t UsbSerial::read(bool *succsess)
{
    if (succsess)
        *succsess = false;
    return 0;
}

void UsbSerial::register_irq(ios_irq_handler_t irq)
{
    m_irq_handler = irq;
    usb_vcp_init(irq);
}
//<<----------------------