#ifndef APP_PROTO_LINK_H
#define APP_PROTO_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "app/proto/proto.h"
#include "libs/Ring-Buffer/ringbuffer.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Колбэк передачи закодированного кадра
     *
     * @param data данные
     * @param size размер данных
     * @param ctx контекст
     * @return true — кадр принят транспортом полностью;
     *         false — транспорт занят, нужно повторить попытку позже
     */
    typedef bool (*proto_write_fn)(const uint8_t *data, uint32_t size,
                                   void *ctx);

    /**
     * @brief Обработчик запроса
     *
     * @param req разобранный запрос
     * @param resp формируемый ответ (заголовок уже заполнен)
     * @param ctx контекст
     */
    typedef void (*proto_handler_fn)(const proto_req_t *req,
                                     proto_resp_t *resp, void *ctx);

    /**
     * @brief Состояние канального уровня протокола
     */
    typedef struct
    {
        uint8_t rx_cobs[PROTO_MAX_COBS_FRAME]; /**< накопление COBS/R */
        uint32_t rx_cobs_len; /**< длина накопленных данных */
        bool rx_dropping; /**< пропуск данных до терминатора */
        uint8_t rx_raw[PROTO_MAX_RAW_FRAME]; /**< раскодированный кадр */
        uint8_t tx_raw[PROTO_MAX_RAW_FRAME]; /**< собираемый ответ */
        uint8_t tx_stage[PROTO_MAX_ENCODED_FRAME]; /**< порция на отправку */
        uint32_t tx_stage_len; /**< длина порции */
        uint16_t tx_retry; /**< счётчик неудачных отправок */
        char tx_ring[PROTO_TX_RING_SIZE]; /**< буфер передачи */
        ring_buffer_t tx_ring_rb; /**< кольцевой буфер передачи */
        proto_write_fn write; /**< колбэк передачи */
        proto_handler_fn handler; /**< обработчик запросов */
        void *ctx; /**< контекст обработчика и передачи */
        proto_stats_t stats; /**< счётчики */
    } proto_link_t;

    /**
     * @brief Инициализировать канальный уровень
     *
     * @param link состояние
     * @param write колбэк передачи
     * @param handler обработчик запросов (может быть NULL)
     * @param ctx контекст
     */
    void proto_link_init(proto_link_t *link, proto_write_fn write,
                         proto_handler_fn handler, void *ctx);

    /**
     * @brief Принять порцию байтов из транспорта
     *
     * Функция потоковая: кадры могут приходить частями и по несколько
     * штук в одной порции. Может вызываться из основного цикла.
     *
     * @param link состояние
     * @param data данные
     * @param size размер порции
     */
    void proto_link_feed(proto_link_t *link, const uint8_t *data,
                         uint32_t size);

    /**
     * @brief Вытолкнуть накопленные ответы в транспорт
     *
     * Вызывается регулярно из основного цикла. Если транспорт занят,
     * отправка повторяется при следующем вызове.
     *
     * @param link состояние
     */
    void proto_link_poll(proto_link_t *link);

    /**
     * @brief Получить счётчики протокола
     *
     * @param link состояние
     * @return const proto_stats_t* счётчики
     */
    const proto_stats_t *proto_link_stats(const proto_link_t *link);

#ifdef __cplusplus
}
#endif

#endif // APP_PROTO_LINK_H