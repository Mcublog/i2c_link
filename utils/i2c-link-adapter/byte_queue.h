#ifndef I2C_LINK_ADAPTER_BYTE_QUEUE_H
#define I2C_LINK_ADAPTER_BYTE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "libs/Ring-Buffer/ringbuffer.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Очередь байтов фиксированной ёмкости
     *
     * Ёмкость должна быть степенью двойки (требование Ring-Buffer).
     * Один байт ёмкости всегда остаётся свободным.
     */
    typedef struct
    {
        ring_buffer_t rb; /**< кольцевой буфер */
        uint8_t *storage; /**< память буфера */
        uint32_t capacity; /**< ёмкость, байт */
        uint32_t dropped; /**< счётчик отброшенных порций */
    } byte_queue_t;

    /**
     * @brief Инициализировать очередь
     *
     * @param queue очередь
     * @param storage память буфера
     * @param capacity ёмкость, байт (степень двойки)
     */
    void byte_queue_init(byte_queue_t *queue, uint8_t *storage,
                         uint32_t capacity);

    /**
     * @brief Положить целую порцию байтов в очередь
     *
     * Порция кладётся только целиком: при нехватке места она
     * отбрасывается, а счётчик dropped увеличивается.
     *
     * @param queue очередь
     * @param data данные
     * @param size размер порции
     * @return true - порция принята
     */
    bool byte_queue_push(byte_queue_t *queue, const uint8_t *data,
                         uint32_t size);

    /**
     * @brief Забрать до cap байтов из очереди
     *
     * @param queue очередь
     * @param out буфер назначения
     * @param cap ёмкость буфера
     * @return uint32_t число извлечённых байтов (0 - очередь пуста)
     */
    uint32_t byte_queue_pop(byte_queue_t *queue, uint8_t *out, uint32_t cap);

    /**
     * @brief Получить число байтов в очереди
     *
     * @param queue очередь
     * @return uint32_t число байтов
     */
    uint32_t byte_queue_used(const byte_queue_t *queue);

    /**
     * @brief Проверить, что очередь пуста
     *
     * @param queue очередь
     * @return true - очередь пуста
     */
    bool byte_queue_empty(const byte_queue_t *queue);

    /**
     * @brief Очистить очередь
     *
     * @param queue очередь
     */
    void byte_queue_clear(byte_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif // I2C_LINK_ADAPTER_BYTE_QUEUE_H