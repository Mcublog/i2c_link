#pragma once

#include <stddef.h>
#include <stdint.h>

#include "main.h"

/** @brief Callback приёма одного байта UART */
typedef void (*uart_rx_callback_t)(uint8_t);

#ifdef HAL_UART_MODULE_ENABLED
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализация UART с приёмом по прерываниям
     *
     * @param huart экземпляр UART из CubeMX
     * @param clb callback приёма байта (может быть NULL)
     */
    void uart_init(UART_HandleTypeDef *huart, uart_rx_callback_t clb);

    /**
     * @brief Блокирующая отправка данных
     *
     * @param huart экземпляр UART из CubeMX
     * @param data данные
     * @param size размер данных
     * @return int 0 - успешно, иначе ошибка
     */
    int uart_send_data(UART_HandleTypeDef *huart, const uint8_t *data,
                       size_t size);

    /**
     * @brief Обработчик прерывания UART (вызывать из HAL_UART_IRQHandler)
     *
     * @param huart экземпляр UART из CubeMX
     */
    void uart_irq_handler(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif
#endif // HAL_UART_MODULE_ENABLED