/**
 * @file i2c_slave_service.c
 * @brief Сервис slave-режима: события шины <-> протокол host
 * @version 0.1
 * @date 2026-11-25
 *
 */
#include "app/i2c/i2c_slave_service.h"

#include <string.h>

#include "app/i2c/islave.h"
#include "app/system/icritical.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME i2cslave
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Размер очереди событий */
#define EVENT_QUEUE_SIZE (4)

/**
 * @brief Событие slave-режима, ожидающее отправки
 */
typedef struct
{
    uint8_t cmd; /**< команда-событие */
    uint8_t flags; /**< флаги события */
    uint8_t addr; /**< адрес slave */
    uint8_t status; /**< статус события */
    uint8_t len; /**< длина payload */
    uint8_t data[PROTO_MAX_DATA]; /**< payload */
} slave_event_t;

/** @brief Кольцевая очередь событий */
static slave_event_t m_queue[EVENT_QUEUE_SIZE];
/** @brief Индекс записи в очередь */
static volatile uint8_t m_head;
/** @brief Индекс чтения из очереди */
static volatile uint8_t m_tail;
/** @brief Канальный уровень протокола */
static proto_link_t *m_link;
/** @brief Счётчик событий (номер сообщения) */
static uint8_t m_seq;
/** @brief Номер события, ожидающего ответа хоста */
static uint8_t m_await_seq;
/** @brief Признак ожидания ответа хоста */
static bool m_await_active;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Поставить событие в очередь
 *
 * @param event событие
 * @return true - событие принято
 */
static bool queue_push(const slave_event_t *event);

/**
 * @brief Прочитать событие из очереди без извлечения
 *
 * @param event буфер события
 * @return true - в очереди есть событие
 */
static bool queue_peek(slave_event_t *event);

/**
 * @brief Извлечь событие из очереди
 */
static void queue_drop(void);

/**
 * @brief Очистить очередь событий
 */
static void queue_clear(void);
//<<----------------------

//>>---------------------- Private
static bool queue_push(const slave_event_t *event)
{
    bool accepted = false;

    critical_enter();
    uint8_t next = (uint8_t)((m_head + 1) % EVENT_QUEUE_SIZE);
    if (next != m_tail)
    {
        m_queue[m_head] = *event;
        m_head = next;
        accepted = true;
    }
    critical_exit();

    return accepted;
}

static bool queue_peek(slave_event_t *event)
{
    bool present = false;

    critical_enter();
    if (m_tail != m_head)
    {
        *event = m_queue[m_tail];
        present = true;
    }
    critical_exit();

    return present;
}

static void queue_drop(void)
{
    critical_enter();
    if (m_tail != m_head)
        m_tail = (uint8_t)((m_tail + 1) % EVENT_QUEUE_SIZE);
    critical_exit();
}

static void queue_clear(void)
{
    critical_enter();
    m_head = 0;
    m_tail = 0;
    critical_exit();
}
//<<----------------------

//>>---------------------- Public
void i2c_slave_service_init(proto_link_t *link)
{
    m_link = link;
    m_seq = 0;
    m_await_seq = 0;
    m_await_active = false;
    queue_clear();
}

void i2c_slave_service_reset(void)
{
    m_await_active = false;
    m_await_seq = 0;
    queue_clear();
}

void i2c_slave_service_poll(void)
{
    slave_event_t event;

    if (m_link == NULL)
        return;

    while (queue_peek(&event))
    {
        uint8_t seq = m_seq;
        bool sent = proto_link_send_event(
            m_link, (proto_cmd_t)event.cmd, seq, event.flags, event.addr,
            (proto_status_t)event.status,
            (event.len != 0) ? event.data : NULL, event.len);

        if (!sent)
            break;

        queue_drop();

        if (event.flags & PROTO_EVENT_FLAG_READ)
        {
            m_await_seq = seq;
            m_await_active = true;
        }

        m_seq = (uint8_t)((m_seq + 1) & PROTO_MSG_ID_MASK);
    }
}

bool i2c_slave_service_busy(void)
{
    bool pending = false;

    critical_enter();
    pending = (m_head != m_tail);
    critical_exit();

    return pending || m_await_active;
}

void i2c_slave_service_on_write(uint8_t addr, const uint8_t *data,
                                uint16_t len, bool read_follows)
{
    slave_event_t event;

    memset(&event, 0, sizeof(event));
    event.cmd = (uint8_t)PROTO_CMD_SLAVE_EVENT;
    event.flags = PROTO_EVENT_FLAG_WRITE;
    if (read_follows)
        event.flags |= PROTO_EVENT_FLAG_READ;
    event.addr = addr;
    event.status = PROTO_STATUS_OK;

    if (len > PROTO_MAX_DATA)
    {
        len = PROTO_MAX_DATA;
        event.status = PROTO_STATUS_OVERFLOW;
    }

    if ((data != NULL) && (len != 0))
        memcpy(event.data, data, len);

    event.len = (uint8_t)len;

    if (!queue_push(&event))
    {
        LOG_WARNING("slave event queue overflow");
    }
}

void i2c_slave_service_on_read(uint8_t addr)
{
    slave_event_t event;

    memset(&event, 0, sizeof(event));
    event.cmd = (uint8_t)PROTO_CMD_SLAVE_EVENT;
    event.flags = PROTO_EVENT_FLAG_READ;
    event.addr = addr;
    event.status = PROTO_STATUS_OK;

    if (!queue_push(&event))
    {
        LOG_WARNING("slave event queue overflow");
    }
}

void i2c_slave_service_on_done(uint8_t addr, uint16_t written, uint16_t read,
                               proto_status_t status)
{
    slave_event_t event;

    memset(&event, 0, sizeof(event));
    event.cmd = (uint8_t)PROTO_CMD_SLAVE_DONE;
    event.flags = 0;
    if (written != 0)
        event.flags |= PROTO_EVENT_FLAG_WRITE;
    if (read != 0)
        event.flags |= PROTO_EVENT_FLAG_READ;
    event.addr = addr;
    event.status = (uint8_t)status;
    event.len = PROTO_SLAVE_DONE_SIZE;
    event.data[0] = (written > 0xFF) ? 0xFF : (uint8_t)written;
    event.data[1] = (read > 0xFF) ? 0xFF : (uint8_t)read;

    if (!queue_push(&event))
    {
        LOG_WARNING("slave event queue overflow");
    }

    m_await_active = false;
}

void i2c_slave_service_on_reply(const proto_req_t *req)
{
    if ((req == NULL) || (req->cmd != PROTO_CMD_SLAVE_EVENT))
        return;

    if (!m_await_active || (req->msg_id != m_await_seq))
    {
        LOG_WARNING("slave reply for unknown event: %u", req->msg_id);
        return;
    }

    m_await_active = false;

    if (!i2c_slave_hw_supply(req->data, req->len))
        LOG_WARNING("slave supply failed");
}
//<<----------------------
