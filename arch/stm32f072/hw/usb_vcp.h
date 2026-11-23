#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "io/types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализация USB VCP транспорта
     *
     * @param handler обработчик принятых данных
     * @return true - успешно
     */
    bool usb_vcp_init(ios_irq_handler_t handler);

    /**
     * @brief Передача данных через USB VCP
     *
     * @param data данные
     * @param size размер данных
     * @return true - пакет принят USB-стеком
     */
    bool usb_vcp_write(const uint8_t *data, uint32_t size);

    /**
     * @brief Проверить, свободен ли передатчик USB VCP
     *
     * Возвращает true, когда предыдущая передача завершена и буфер
     * данных больше не используется USB-стеком.
     *
     * @return true - можно начинать новую передачу
     */
    bool usb_vcp_tx_ready(void);

    /**
     * @brief Обработчик принятых по USB данных
     *
     * Вызывается из CDC_Receive_FS (USER CODE).
     *
     * @param chunk принятый фрагмент
     */
    void usb_vcp_irq_handler(ios_chunk_t *chunk);

#ifdef __cplusplus
}
#endif