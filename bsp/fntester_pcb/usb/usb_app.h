#ifndef BSP_FNTESTER_PCB_USB_USB_APP_H
#define BSP_FNTESTER_PCB_USB_USB_APP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализировать USB-устройство (TinyUSB)
     *
     * Включает тактирование USB и запускает стек устройства. Составное
     * устройство: CDC (виртуальный COM-порт) и vendor-интерфейс для
     * протокола i2c-tiny-usb.
     *
     * @return true - стек запущен
     */
    bool usb_app_init(void);

    /**
     * @brief Обработать события USB-стека
     *
     * Вызывается регулярно из главного цикла. В контексте этой функции
     * выполняются колбэки классов, включая обработку control-запросов.
     */
    void usb_app_poll(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_FNTESTER_PCB_USB_USB_APP_H
