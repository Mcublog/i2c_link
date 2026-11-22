#pragma once

#include <stddef.h>
#include <stdint.h>

typedef void (*uart2_rx_callback_t)(uint8_t);

#ifdef __cplusplus
extern "C"
{
#endif

    void uart2_init(uart2_rx_callback_t clb);
    int uart2_send_data(const uint8_t *data, size_t s);
    void uart2_irq_handler(void);

#ifdef __cplusplus
}
#endif
