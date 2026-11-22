#ifndef APP_UTILS_ITICK_TIMER_H
#define APP_UTILS_ITICK_TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Запуск микросекундного таймера
     *
     * @param header указатель на TIM_HandleTypeDef (таймер 1 МГц)
     */
    void tt_init(void *header);

    /**
     * @brief Задержка в микросекундах на таймере
     *
     * @param header указатель на TIM_HandleTypeDef
     * @param us число микросекунд
     */
    void tt_delay_us(void *header, uint16_t us);

    /**
     * @brief Сброс счётчика таймера
     *
     * @param header указатель на TIM_HandleTypeDef
     */
    void tt_reset(void *header);

    /**
     * @brief Текущее значение счётчика таймера
     *
     * @param header указатель на TIM_HandleTypeDef
     * @return uint16_t значение счётчика
     */
    uint16_t tt_read(void *header);

#ifdef __cplusplus
}
#endif

#endif // APP_UTILS_ITICK_TIMER_H