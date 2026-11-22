
#ifndef APP_IO_DEBOUNCE_H
#define APP_IO_DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/io/types.h"
#include "libs/Ring-Buffer/ringbuffer.h"

#define DEBOUNCE_SIZE (8)

/** @brief Структура дебаунсера */
typedef struct debounce
{
    uint8_t buffer[DEBOUNCE_SIZE]; /**< буфер отсчётов */
    ring_buffer_t rb; /**< кольцевой буфер */
    uint16_t state_counter; /**< счётчик стабильного состояния */
    bool last_state; /**< последнее устойчивое состояние */
    uint8_t actual_size; /**< фактический размер буфера */
} debouncer_t;


#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Инициализировать дебаунсер с размером по умолчанию (DEBOUNCE_SIZE)
     *
     * @param db дебаунсер
     * @param state начальное состояние
     * @return true OK, false ошибка
     */
    bool dbnc_init(debouncer_t *db, bool state);

    /**
     * @brief Инициализировать дебаунсер с указанным размером буфера.
     * size <= DEBOUNCE_SIZE и должен быть кратен 2 или равен 1.
     * При size == 1 антидребезг фактически не работает
     *
     * @param db дебаунсер
     * @param state начальное состояние
     * @param size размер буфера (1..DEBOUNCE_SIZE, кратен 2 или равен 1)
     * @return true OK, false использован размер по умолчанию
     */
    bool dbnc_init_with_size(debouncer_t *db, bool state, uint8_t size);

    /**
     * @brief Установить текущее состояние GPIO
     *
     * @param db дебаунсер
     * @param state состояние GPIO
     */
    void dbnc_set_current_state(debouncer_t *db, bool state);

    /**
     * @brief Получить устойчивое состояние
     *
     * @param db дебаунсер
     * @return true высокий уровень, false низкий
     */
    bool dbnc_get_state(const debouncer_t *db);

    /**
     * @brief Получить тип фронта/ уровня
     *
     * @param db дебаунсер
     * @return egde_t тип сигнала
     */
    egde_t dbnc_get_edge(debouncer_t *db);

    /**
     * @brief Получить фактический размер буфера
     *
     * @param db дебаунсер
     * @return uint8_t размер буфера
     */
    uint8_t dbnc_get_actual_size(const debouncer_t *db);

    /**
     * @brief Проверить, что кольцевой буфер заполнен адекватными значениями
     *
     * @param db дебаунсер
     * @return true буфер полон, false иначе
     */
    bool dbnc_is_ready(const debouncer_t *db);

    /**
     * @brief Получить счётчик стабильного состояния
     *
     * @param db дебаунсер
     * @return uint16_t значение счётчика
     */
    uint16_t dgnc_state_counter(const debouncer_t *db);

    /**
     * @brief Сбросить счётчик стабильного состояния
     *
     * @param db дебаунсер
     */
    void dgnc_reset_state_counter(debouncer_t *db);

#ifdef __cplusplus
}
#endif

#endif // APP_SENSORS_DEBOUNCE_H