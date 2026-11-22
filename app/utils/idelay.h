#ifndef IDELAY_H
#define IDELAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Задержка в миллисекундах (busy-wait)
     *
     * @param ms число миллисекунд
     */
    void delay_ms(uint32_t ms);

    /**
     * @brief Задержка в микросекундах (busy-wait)
     *
     * @param us число микросекунд
     */
    void delay_us(uint16_t us);

#ifdef __cplusplus
}
#endif

#endif // IDELAY_H
