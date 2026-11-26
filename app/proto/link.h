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
     * @brief Обработчик кадра от устройства
     *
     * Вызывается для кадров ответного формата (бит направления в msg_id
     * установлен): ответов на команды и событий slave-режима.
     *
     * @param evt разобранный кадр
     * @param ctx контекст
     */
    typedef void (*proto_event_fn)(const proto_resp_t *evt, void *ctx);

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
        proto_event_fn event; /**< обработчик кадров от устройства */
        void *ctx; /**< контекст обработчиков и передачи */
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
     * @brief Назначить обработчик кадров от устройства
     *
     * Для кадров, приходящих с битом направления в msg_id (ответы и
     * события slave-режима), вместо обработчика запросов вызывается
     * этот колбэк. Если он не назначен, поведение прежнее: кадр
     * отбрасывается, а на канал уходит ответ с ошибкой FRAME.
     *
     * @param link состояние
     * @param event обработчик кадров от устройства (может быть NULL)
     */
    void proto_link_set_event_handler(proto_link_t *link,
                                      proto_event_fn event);

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
     * @brief Поставить в очередь произвольный сырой кадр
     *
     * Кадр кодируется в COBS/R и уходит в транспорт при следующем
     * вызове proto_link_poll(). Используется сторонами, которые сами
     * собрали кадр (например, хост-адаптер slave-режима).
     *
     * @param link состояние
     * @param raw сырой кадр (без COBS/R и терминатора)
     * @param size размер кадра
     * @return true - кадр поставлен в очередь
     */
    bool proto_link_send_frame(proto_link_t *link, const uint8_t *raw,
                               uint32_t size);

    /**
     * @brief Поставить в очередь кадр, инициированный устройством
     *
     * Используется для событий slave-режима: направление в поле msg_id
     * выставляется автоматически (бит 7), кадр уходит в транспорт при
     * следующем вызове proto_link_poll().
     *
     * @param link состояние
     * @param cmd команда-событие
     * @param msg_id номер события (0..127, без бита направления)
     * @param flags флаги события
     * @param addr адрес slave, к которому относится событие
     * @param status статус/результат события
     * @param data payload (может быть NULL)
     * @param len длина payload
     * @return true - кадр поставлен в очередь
     */
    bool proto_link_send_event(proto_link_t *link, proto_cmd_t cmd,
                               uint8_t msg_id, uint8_t flags, uint8_t addr,
                               proto_status_t status, const uint8_t *data,
                               uint8_t len);

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