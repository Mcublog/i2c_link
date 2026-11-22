
#ifndef APP_SYSTEM_IGPIO_H
#define APP_SYSTEM_IGPIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Инициализация GPIO (LED, кнопки и т.д.)
     */
    void io_gpio_init(void);

    /**
     * @brief Управление светодиодом LED1
     *
     * @param true включить, false выключить
     */
    void io_gpio_led1(bool on);

#ifdef __cplusplus
}
#endif

#endif // APP_SYSTEM_IGPIO_H