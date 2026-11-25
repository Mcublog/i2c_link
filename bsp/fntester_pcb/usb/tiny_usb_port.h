#ifndef BSP_FNTESTER_PCB_USB_TINY_USB_PORT_H
#define BSP_FNTESTER_PCB_USB_TINY_USB_PORT_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализировать мост i2c-tiny-usb
     *
     * Сбрасывает состояние моста. Вызывается при старте USB-устройства.
     */
    void tiny_usb_port_init(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_FNTESTER_PCB_USB_TINY_USB_PORT_H
