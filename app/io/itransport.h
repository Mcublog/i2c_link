#ifndef APP_IO_ITRANSPORT_H
#define APP_IO_ITRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Обработчик принятых из транспорта данных
     *
     * Может вызываться из контекста прерывания (прошивка) или из потока
     * чтения (desktop). Обработчик должен быть коротким и не выполнять
     * блокирующих операций.
     *
     * @param data данные
     * @param size размер данных
     */
    typedef void (*transport_rx_handler_t)(const uint8_t *data,
                                           uint32_t size);

    /**
     * @brief Инициализировать транспорт
     *
     * @param handler обработчик принятых данных
     * @return true - успешно
     */
    bool transport_init(transport_rx_handler_t handler);

    /**
     * @brief Передать данные в транспорт
     *
     * @param data данные
     * @param size размер данных
     * @return true - данные приняты полностью;
     *         false - транспорт занят, нужно повторить позже
     */
    bool transport_write(const uint8_t *data, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // APP_IO_ITRANSPORT_H