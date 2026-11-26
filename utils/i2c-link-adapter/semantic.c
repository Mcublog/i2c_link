/**
 * @file semantic.c
 * @brief Семантический режим: события slave-режима <-> простой текст
 * @version 0.1
 * @date 2026-11-26
 *
 */
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/proto/frame.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME sem
#define LOG_MODULE_LEVEL (4)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Число байт в hex-строке */
#define HEX_BYTE_LEN (2)
//<<----------------------

//>>---------------------- Private
/**
 * @brief Отправить строку клиенту
 *
 * @param sem адаптер
 * @param text строка
 * @return true - строка принята
 */
static bool send_text(semantic_t *sem, const char *text)
{
    if ((sem == NULL) || (sem->text_out == NULL) || (text == NULL))
        return false;

    return sem->text_out(sem->ctx, text, (uint32_t)strlen(text));
}

/**
 * @brief Закодировать байты в hex-строку
 *
 * @param data данные
 * @param len длина
 * @param out буфер результата (не менее 2 * len + 1)
 * @param cap ёмкость буфера
 * @return uint32_t длина строки
 */
static uint32_t to_hex(const uint8_t *data, uint32_t len, char *out,
                       uint32_t cap)
{
    static const char kDigits[] = "0123456789ABCDEF";
    uint32_t pos = 0;

    for (uint32_t i = 0; (i < len) && ((pos + HEX_BYTE_LEN) < cap); ++i)
    {
        out[pos++] = kDigits[(data[i] >> 4) & 0x0F];
        out[pos++] = kDigits[data[i] & 0x0F];
    }
    out[pos] = '\0';
    return pos;
}

/**
 * @brief Преобразовать hex-символ в число
 *
 * @param symbol символ
 * @return int значение или -1
 */
static int hex_digit(char symbol)
{
    if ((symbol >= '0') && (symbol <= '9'))
        return symbol - '0';
    if ((symbol >= 'A') && (symbol <= 'F'))
        return symbol - 'A' + 10;
    if ((symbol >= 'a') && (symbol <= 'f'))
        return symbol - 'a' + 10;
    return -1;
}

/**
 * @brief Разобрать hex-строку в байты
 *
 * @param text строка
 * @param data буфер результата
 * @param cap ёмкость буфера
 * @return int длина результата или -1 при ошибке
 */
static int from_hex(const char *text, uint8_t *data, uint32_t cap)
{
    size_t len = strlen(text);

    if ((len % HEX_BYTE_LEN) != 0)
        return -1;
    if ((len / HEX_BYTE_LEN) > cap)
        return -1;

    for (size_t i = 0; i < len; i += HEX_BYTE_LEN)
    {
        int hi = hex_digit(text[i]);
        int lo = hex_digit(text[i + 1]);
        if ((hi < 0) || (lo < 0))
            return -1;
        data[i / HEX_BYTE_LEN] = (uint8_t)((hi << 4) | lo);
    }

    return (int)(len / HEX_BYTE_LEN);
}

/**
 * @brief Сформировать и отправить ответ хоста устройству
 *
 * @param sem адаптер
 * @param msg_id номер ожидающего события
 * @param addr адрес slave
 * @param data байты для фазы чтения (может быть NULL)
 * @param len длина байтов
 * @return true - ответ записан в CDC
 */
static bool send_reply(semantic_t *sem, uint8_t msg_id, uint8_t addr,
                       const uint8_t *data, uint32_t len)
{
    proto_req_t req;
    uint8_t raw[PROTO_MAX_RAW_FRAME];

    memset(&req, 0, sizeof(req));
    req.cmd = PROTO_CMD_SLAVE_EVENT;
    req.msg_id = msg_id;
    req.addr = addr;
    req.len = (uint8_t)len;
    req.data = (len != 0) ? data : NULL;

    uint32_t size = proto_frame_build_req(raw, sizeof(raw), &req);
    if (size == 0)
        return false;

    if (sem->raw_out == NULL)
        return false;

    return sem->raw_out(sem->ctx, raw, size);
}

/**
 * @brief Закрыть ожидание события
 *
 * @param sem адаптер
 */
static void clear_pending(semantic_t *sem)
{
    sem->pending = false;
    sem->read_follows = false;
    sem->msg_id = 0;
    sem->addr = 0;
    sem->flags = 0;
    sem->deadline_ms = 0;
}

/**
 * @brief Отправить устройству пустой ответ (эквивалент 0xFF)
 *
 * @param sem адаптер
 */
static void send_fallback(semantic_t *sem)
{
    // NOTE: пустой ответ устройство трактует как байт 0xFF и отпускает SCL
    if (!send_reply(sem, sem->msg_id, sem->addr, NULL, 0))
        LOG_WARNING("reply drop: cdc busy or link error");

    clear_pending(sem);
}

/**
 * @brief Обработать команду клиента
 *
 * Поддерживаются REPLY и STOP. Остальные строки отбрасываются
 * с ответом ERR CMD.
 *
 * @param sem адаптер
 * @param line строка без перевода строки
 */
static void handle_line(semantic_t *sem, char *line)
{
    char *arg = NULL;

    if ((line[0] == '\0') || (line[0] == '#'))
        return;

    char *space = strchr(line, ' ');
    if (space != NULL)
    {
        *space = '\0';
        arg = space + 1;
        while (*arg == ' ')
            arg++;
    }

    if (strcmp(line, "REPLY") == 0)
    {
        if (!sem->pending)
        {
            send_text(sem, "ERR NOPENDING\r\n");
            return;
        }

        if (arg == NULL)
        {
            sem->malformed++;
            send_text(sem, "ERR ARG\r\n");
            return;
        }

        char *end = NULL;
        unsigned long id = strtoul(arg, &end, 0);
        if ((end == arg) || (id > PROTO_MSG_ID_MASK))
        {
            sem->malformed++;
            send_text(sem, "ERR ARG\r\n");
            return;
        }

        while (*end == ' ')
            end++;

        if (id != sem->msg_id)
        {
            // NOTE: ответ на уже закрытое событие не отправляем
            sem->malformed++;
            send_text(sem, "ERR ID\r\n");
            return;
        }

        uint8_t data[PROTO_MAX_DATA];
        int len = 0;
        if (end[0] != '\0')
        {
            len = from_hex(end, data, sizeof(data));
            if (len < 0)
            {
                sem->malformed++;
                send_text(sem, "ERR BADHEX\r\n");
                return;
            }
        }

        if (send_reply(sem, sem->msg_id, sem->addr, data, (uint32_t)len))
            sem->answered++;
        else
            LOG_WARNING("reply drop: cdc busy or link error");

        clear_pending(sem);
        send_text(sem, "OK\r\n");
        return;
    }

    if (strcmp(line, "STOP") == 0)
    {
        if (sem->pending)
            send_fallback(sem);
        send_text(sem, "OK\r\n");
        return;
    }

    LOG_DEBUG("unknown command: %s", line);
    sem->malformed++;
    send_text(sem, "ERR CMD\r\n");
}
//<<----------------------

//>>---------------------- Public
void semantic_init(semantic_t *sem, uint16_t reply_timeout_ms,
                   sem_text_fn text_out, sem_raw_fn raw_out, void *ctx)
{
    if (sem == NULL)
        return;

    memset(sem, 0, sizeof(*sem));
    sem->reply_timeout_ms = reply_timeout_ms;
    sem->text_out = text_out;
    sem->raw_out = raw_out;
    sem->ctx = ctx;
}

void semantic_on_frame(semantic_t *sem, const proto_resp_t *evt,
                       uint64_t now_ms)
{
    char line[SEM_LINE_MAX];
    char hex[PROTO_MAX_DATA * 2 + 1];

    if ((sem == NULL) || (evt == NULL))
        return;

    switch (evt->cmd)
    {
    case PROTO_CMD_SLAVE_EVENT:
        sem->events++;

        if (sem->pending)
        {
            // NOTE: устройство на событие с ожиданием ответа не присылает
            // новые события; если это всё же случилось — прежнему отвечаем
            // пустым, чтобы не держать шину
            LOG_WARNING("pending event replaced by %u", evt->msg_id);
            send_fallback(sem);
        }

        // NOTE: ответ нужен только для фазы чтения, иначе мастер уже
        // отпустил шину и устройство ничего не ждёт
        sem->pending = (evt->flags & PROTO_EVENT_FLAG_READ) != 0;
        sem->read_follows = sem->pending;
        sem->msg_id = evt->msg_id;
        sem->addr = evt->addr;
        sem->flags = evt->flags;
        // NOTE: 0 отключает автоматический ответ по таймауту
        sem->deadline_ms = (sem->reply_timeout_ms == 0)
                               ? 0
                               : (now_ms + sem->reply_timeout_ms);

        const char *kind = "W";
        if ((evt->flags & PROTO_EVENT_FLAG_WRITE) != 0)
            kind = ((evt->flags & PROTO_EVENT_FLAG_READ) != 0) ? "WR" : "W";
        else
            kind = ((evt->flags & PROTO_EVENT_FLAG_READ) != 0) ? "R" : "-";

        if (evt->len != 0)
        {
            (void)to_hex(evt->data, evt->len, hex, sizeof(hex));
            (void)snprintf(line, sizeof(line), "EVENT %u %s %02X %s\r\n",
                           evt->msg_id, kind, evt->addr, hex);
        }
        else
        {
            (void)snprintf(line, sizeof(line), "EVENT %u %s %02X\r\n",
                           evt->msg_id, kind, evt->addr);
        }

        if (!send_text(sem, line))
            LOG_WARNING("event drop: client socket busy");

        if (evt->status == PROTO_STATUS_OVERFLOW)
            (void)send_text(sem, "WARN OVERFLOW\r\n");
        break;

    case PROTO_CMD_SLAVE_DONE:
        sem->done++;
        if ((evt->len >= PROTO_SLAVE_DONE_SIZE) && (evt->data != NULL))
        {
            (void)snprintf(line, sizeof(line), "DONE W=%u R=%u %02X\r\n",
                           evt->data[0], evt->data[1], (unsigned)evt->status);
        }
        else
        {
            (void)snprintf(line, sizeof(line), "DONE -- -- %02X\r\n",
                           (unsigned)evt->status);
        }
        if (!send_text(sem, line))
            LOG_WARNING("done drop: client socket busy");
        break;

    default:
        sem->other++;
        LOG_DEBUG("pass-through frame cmd 0x%02X", (unsigned)evt->cmd);
        break;
    }
}

void semantic_feed(semantic_t *sem, const uint8_t *data, uint32_t size,
                   uint64_t now_ms)
{
    (void)now_ms;

    if ((sem == NULL) || (data == NULL))
        return;

    for (uint32_t i = 0; i < size; ++i)
    {
        char symbol = (char)data[i];

        if ((symbol == '\n') || (symbol == '\r'))
        {
            if (sem->text_len != 0)
            {
                sem->text[sem->text_len] = '\0';
                handle_line(sem, sem->text);
                sem->text_len = 0;
            }
            continue;
        }

        if (sem->text_len < (sizeof(sem->text) - 1))
        {
            sem->text[sem->text_len++] = symbol;
        }
        else
        {
            // NOTE: слишком длинная строка — сбрасываем до перевода строки
            sem->text_len = 0;
            sem->malformed++;
            send_text(sem, "ERR TOOLONG\r\n");
        }
    }
}

void semantic_poll(semantic_t *sem, uint64_t now_ms)
{
    if ((sem == NULL) || !sem->pending)
        return;
    if (sem->reply_timeout_ms == 0)
        return;
    if (now_ms < sem->deadline_ms)
        return;

    sem->timeout++;
    LOG_WARNING("event %u unanswered, replying 0xFF", sem->msg_id);
    send_text(sem, "TIMEOUT\r\n");
    send_fallback(sem);
}

void semantic_cancel(semantic_t *sem)
{
    if ((sem == NULL) || !sem->pending)
        return;

    LOG_WARNING("client lost, event %u answered with 0xFF", sem->msg_id);
    send_fallback(sem);
}

void semantic_drop(semantic_t *sem)
{
    if (sem == NULL)
        return;

    clear_pending(sem);
}

bool semantic_pending(const semantic_t *sem)
{
    return (sem != NULL) && sem->pending;
}
//<<----------------------