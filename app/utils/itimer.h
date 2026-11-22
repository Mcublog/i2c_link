#ifndef APP_UTILS_ITIMER_H
#define APP_UTILS_ITIMER_H

/** @brief Тип callback-функции таймера (вызывается из SysTick) */
typedef void (*timer_systick_callback_t)(void);

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Инициализация аппаратного таймера
     *
     * @param systick_callback функция, вызываемая по SysTick (1 мс)
     */
    void app_timer_init(timer_systick_callback_t systick_callback);

    /**
     * @brief Разрешить прерывания таймера
     */
    void app_timer_irq_enable(void);

    /**
     * @brief Запретить прерывания таймера
     */
    void app_timer_irq_disable(void);

#ifdef __cplusplus
}
#endif

#endif // APP_UTILS_ITIMER_H
