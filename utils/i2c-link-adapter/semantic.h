#ifndef I2C_LINK_ADAPTER_SEMANTIC_H
#define I2C_LINK_ADAPTER_SEMANTIC_H

#include <stdbool.h>
#include <stdint.h>

#include "app/proto/proto.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Максимальная длина текстовой строки в обе стороны */
    #define SEM_LINE_MAX (512)

    /**
     * @brief Отправить текст клиенту
     *
     * @param ctx контекст
     * @param text строка (передаётся целиком, перевод строки включён)
     * @param len длина строки
     * @return true - строка принята
     */
    typedef bool (*sem_text_fn)(void *ctx, const char *text, uint32_t len);

    /**
     * @brief Записать ответ хост -> устройство в CDC
     *
     * @param ctx контекст
     * @param data данные
     * @param size размер данных
     * @return true - данные приняты
     */
    typedef bool (*sem_raw_fn)(void *ctx, const uint8_t *data, uint32_t size);

    /**
     * @brief Состояние семантического адаптера
     */
    typedef struct
    {
        bool pending; /**< есть событие, ожидающее ответа клиента */
        uint8_t msg_id; /**< номер ожидающего события */
        uint8_t addr; /**< адрес slave ожидающего события */
        uint8_t flags; /**< флаги ожидающего события */
        bool read_follows; /**< устройство ждёт байты для фазы чтения */
        uint64_t deadline_ms; /**< срок ответа, мс */
        uint16_t reply_timeout_ms; /**< настроенный срок ответа, мс */
        uint32_t answered; /**< ответов клиента передано устройству */
        uint32_t timeout; /**< ответов по таймауту */
        uint32_t malformed; /**< некорректных строк клиента */
        uint32_t events; /**< переданных клиенту событий */
        uint32_t done; /**< переданных клиенту завершений */
        uint32_t other; /**< прочих кадров от устройства */
        char text[SEM_LINE_MAX]; /**< сборка строки клиента */
        uint32_t text_len; /**< длина накопленной строки */
        sem_text_fn text_out; /**< вывод клиенту */
        sem_raw_fn raw_out; /**< вывод в CDC */
        void *ctx; /**< контекст */
    } semantic_t;

    /**
     * @brief Инициализировать адаптер
     *
     * @param sem адаптер
     * @param reply_timeout_ms срок ответа клиента, мс
     * @param text_out вывод строк клиенту
     * @param raw_out вывод кадров в CDC
     * @param ctx контекст
     */
    void semantic_init(semantic_t *sem, uint16_t reply_timeout_ms,
                       sem_text_fn text_out, sem_raw_fn raw_out, void *ctx);

    /**
     * @brief Обработать кадр от устройства
     *
     * @param sem адаптер
     * @param evt разобранный кадр
     * @param now_ms текущее время, мс
     */
    void semantic_on_frame(semantic_t *sem, const proto_resp_t *evt,
                           uint64_t now_ms);

    /**
     * @brief Принять порцию текста от клиента
     *
     * Строки разделяются \n или \r\n. Некорректные строки отбрасываются.
     *
     * @param sem адаптер
     * @param data данные
     * @param size размер порции
     * @param now_ms текущее время, мс
     */
    void semantic_feed(semantic_t *sem, const uint8_t *data, uint32_t size,
                       uint64_t now_ms);

    /**
     * @brief Обслужить истёкший срок ответа
     *
     * Если клиент не ответил вовремя, устройству отправляется пустой
     * ответ, чтобы не удерживать SCL.
     *
     * @param sem адаптер
     * @param now_ms текущее время, мс
     */
    void semantic_poll(semantic_t *sem, uint64_t now_ms);

    /**
     * @brief Отменить ожидание ответа (клиент отключился)
     *
     * Устройству сразу отправляется пустой ответ.
     *
     * @param sem адаптер
     */
    void semantic_cancel(semantic_t *sem);

    /**
     * @brief Сбросить ожидание без отправки ответа
     *
     * Используется при потере CDC-порта, когда отправлять некуда.
     *
     * @param sem адаптер
     */
    void semantic_drop(semantic_t *sem);

    /**
     * @brief Проверить, есть ли событие, ожидающее ответа
     *
     * @param sem адаптер
     * @return true - ответ ожидается
     */
    bool semantic_pending(const semantic_t *sem);

#ifdef __cplusplus
}
#endif

#endif // I2C_LINK_ADAPTER_SEMANTIC_H