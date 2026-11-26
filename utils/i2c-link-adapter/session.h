#ifndef I2C_LINK_ADAPTER_SESSION_H
#define I2C_LINK_ADAPTER_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "app/proto/proto.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Таймаут ответа на команду настройки, мс */
    #define SESSION_REPLY_TIMEOUT_MS (300)
    /** @brief Число попыток отправки команды настройки */
    #define SESSION_RETRY_LIMIT (3)

    /**
     * @brief Состояние сеанса настройки
     */
    typedef enum
    {
        SESSION_IDLE = 0, /**< не запущен */
        SESSION_WAIT_REPLY, /**< ждём ответ на команду */
        SESSION_READY, /**< режим настроен */
        SESSION_FAILED /**< настройка не удалась */
    } session_state_t;

    /**
     * @brief Колбэк отправки сырого кадра в CDC
     *
     * @param ctx контекст
     * @param raw сырой кадр
     * @param size размер кадра
     * @return true - кадр поставлен в очередь
     */
    typedef bool (*session_send_fn)(void *ctx, const uint8_t *raw,
                                    uint32_t size);

    /**
     * @brief Сеанс настройки режима устройства после подключения CDC
     */
    typedef struct
    {
        session_state_t state; /**< состояние */
        bool enabled; /**< настройка включена */
        uint8_t desired_mode; /**< нужный режим (PROTO_MODE_*) */
        uint8_t desired_addr; /**< нужный адрес slave */
        uint8_t observed_mode; /**< прочитанный режим */
        uint8_t observed_addr; /**< прочитанный адрес */
        uint8_t step; /**< номер текущего шага */
        uint8_t msg_id; /**< номер последнего запроса */
        uint8_t retries; /**< число неудачных попыток */
        uint64_t deadline_ms; /**< срок ожидания ответа */
        uint32_t timeouts; /**< счётчик таймаутов */
        session_send_fn send; /**< колбэк отправки */
        void *ctx; /**< контекст колбэка */
    } session_t;

    /**
     * @brief Инициализировать сеанс настройки
     *
     * @param session сеанс
     * @param enabled false - настройка не выполняется
     * @param mode нужный режим (PROTO_MODE_MASTER/SLAVE)
     * @param addr нужный адрес slave
     * @param send колбэк отправки кадров
     * @param ctx контекст колбэка
     */
    void session_init(session_t *session, bool enabled, uint8_t mode,
                      uint8_t addr, session_send_fn send, void *ctx);

    /**
     * @brief Запустить сеанс настройки
     *
     * Первый запрос отправляется сразу.
     *
     * @param session сеанс
     * @param now_ms текущее время, мс
     */
    void session_start(session_t *session, uint64_t now_ms);

    /**
     * @brief Обслуживать таймауты и повторные попытки
     *
     * @param session сеанс
     * @param now_ms текущее время, мс
     */
    void session_poll(session_t *session, uint64_t now_ms);

    /**
     * @brief Обработать кадр от устройства
     *
     * Кадры, не относящиеся к текущему шагу, игнорируются.
     *
     * @param session сеанс
     * @param evt разобранный кадр
     * @param now_ms текущее время, мс
     */
    void session_on_frame(session_t *session, const proto_resp_t *evt,
                          uint64_t now_ms);

    /**
     * @brief Проверить, идёт ли настройка
     *
     * @param session сеанс
     * @return true - сеанс активен
     */
    bool session_active(const session_t *session);

    /**
     * @brief Проверить, что режим успешно настроен
     *
     * @param session сеанс
     * @return true - настройка завершена успешно
     */
    bool session_ready(const session_t *session);

    /**
     * @brief Прервать настройку (например, при потере CDC-порта)
     *
     * @param session сеанс
     */
    void session_abort(session_t *session);

#ifdef __cplusplus
}
#endif

#endif // I2C_LINK_ADAPTER_SESSION_H