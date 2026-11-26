/**
 * @file session.c
 * @brief Настройка режима устройства после подключения CDC
 * @version 0.1
 * @date 2026-11-26
 *
 */
#include "session.h"

#include <string.h>

#include "app/proto/frame.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME sess
#define LOG_MODULE_LEVEL (4)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/**
 * @brief Шаги настройки
 */
typedef enum
{
    STEP_GET_CONFIG = 0, /**< прочитать текущий режим */
    STEP_SET_ADDR, /**< задать адрес slave */
    STEP_SET_MODE, /**< переключить режим */
    STEP_VERIFY /**< проверить результат */
} session_step_t;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Отправить кадр запроса
 *
 * @param session сеанс
 * @param cmd команда
 * @param data payload
 * @param len длина payload
 * @return true - кадр поставлен в очередь
 */
static bool send_request(session_t *session, proto_cmd_t cmd,
                         const uint8_t *data, uint8_t len)
{
    proto_req_t req;
    uint8_t raw[PROTO_MAX_RAW_FRAME];

    memset(&req, 0, sizeof(req));
    session->msg_id = (uint8_t)((session->msg_id + 1) & PROTO_MSG_ID_MASK);
    req.cmd = cmd;
    req.msg_id = session->msg_id;
    req.len = len;
    req.data = data;

    uint32_t size = proto_frame_build_req(raw, sizeof(raw), &req);
    if (size == 0)
        return false;

    return session->send(session->ctx, raw, size);
}

/**
 * @brief Отправить запрос текущего шага
 *
 * @param session сеанс
 * @param now_ms текущее время, мс
 */
static void start_step(session_t *session, uint64_t now_ms)
{
    bool sent = false;

    switch ((session_step_t)session->step)
    {
    case STEP_GET_CONFIG:
        sent = send_request(session, PROTO_CMD_GET_CONFIG, NULL, 0);
        break;
    case STEP_SET_ADDR:
        sent = send_request(session, PROTO_CMD_SET_SLAVE_ADDR,
                            &session->desired_addr, 1);
        break;
    case STEP_SET_MODE:
        sent = send_request(session, PROTO_CMD_SET_MODE,
                            &session->desired_mode, 1);
        break;
    case STEP_VERIFY:
        sent = send_request(session, PROTO_CMD_GET_CONFIG, NULL, 0);
        break;
    default:
        break;
    }

    if (!sent)
    {
        LOG_ERROR("session: send failed");
        session->state = SESSION_FAILED;
        return;
    }

    session->state = SESSION_WAIT_REPLY;
    session->deadline_ms = now_ms + SESSION_REPLY_TIMEOUT_MS;
}

/**
 * @brief Перейти к следующему шагу
 *
 * @param session сеанс
 * @param now_ms текущее время, мс
 */
static void next_step(session_t *session, uint64_t now_ms)
{
    session->retries = 0;
    session->step++;

    if (session->step > (uint8_t)STEP_VERIFY)
    {
        session->state = SESSION_READY;
        return;
    }

    start_step(session, now_ms);
}

/**
 * @brief Проверить ответ на GET_CONFIG относительно нужной конфигурации
 *
 * @param session сеанс
 * @return true - конфигурация совпадает
 */
static bool config_matches(const session_t *session)
{
    return (session->observed_mode == session->desired_mode) &&
           (session->observed_addr == session->desired_addr);
}
//<<----------------------

//>>---------------------- Public
void session_init(session_t *session, bool enabled, uint8_t mode, uint8_t addr,
                  session_send_fn send, void *ctx)
{
    if (session == NULL)
        return;

    memset(session, 0, sizeof(*session));
    session->state = SESSION_IDLE;
    session->enabled = enabled;
    session->desired_mode = mode;
    session->desired_addr = addr;
    session->send = send;
    session->ctx = ctx;
}

void session_start(session_t *session, uint64_t now_ms)
{
    if (session == NULL)
        return;

    if (!session->enabled)
    {
        LOG_DEBUG("session: configuration disabled");
        session->state = SESSION_READY;
        return;
    }

    session->step = (uint8_t)STEP_GET_CONFIG;
    session->retries = 0;
    session->timeouts = 0;
    LOG_INFO("session: target mode %u, addr 0x%02X", session->desired_mode,
             session->desired_addr);
    start_step(session, now_ms);
}

void session_poll(session_t *session, uint64_t now_ms)
{
    if ((session == NULL) || (session->state != SESSION_WAIT_REPLY))
        return;
    if (now_ms < session->deadline_ms)
        return;

    session->timeouts++;

    if (session->retries >= SESSION_RETRY_LIMIT)
    {
        LOG_ERROR("session: device does not answer");
        session->state = SESSION_FAILED;
        return;
    }

    session->retries++;
    LOG_WARNING("session: retry %u for step %u", session->retries,
                session->step);
    start_step(session, now_ms);
}

void session_on_frame(session_t *session, const proto_resp_t *evt,
                      uint64_t now_ms)
{
    if ((session == NULL) || (evt == NULL))
        return;
    if (session->state != SESSION_WAIT_REPLY)
        return;
    if (evt->msg_id != session->msg_id)
        return;
    if ((evt->cmd != PROTO_CMD_GET_CONFIG) &&
        (evt->cmd != PROTO_CMD_SET_SLAVE_ADDR) &&
        (evt->cmd != PROTO_CMD_SET_MODE))
        return;

    if (evt->status == PROTO_STATUS_NOT_SUPPORTED)
    {
        LOG_ERROR("session: device rejected %s",
                  (evt->cmd == PROTO_CMD_SET_MODE) ? "slave mode" : "command");
        session->state = SESSION_FAILED;
        return;
    }

    if (evt->cmd == PROTO_CMD_GET_CONFIG)
    {
        if (evt->len != PROTO_CONFIG_SIZE)
        {
            LOG_ERROR("session: bad GET_CONFIG payload");
            session->state = SESSION_FAILED;
            return;
        }

        session->observed_mode = evt->data[0];
        session->observed_addr = evt->data[1];
        LOG_INFO("session: device mode %u, addr 0x%02X",
                 session->observed_mode, session->observed_addr);

        if (session->step == (uint8_t)STEP_GET_CONFIG)
        {
            if (config_matches(session))
            {
                LOG_INFO("session: device already configured");
                session->state = SESSION_READY;
                return;
            }

            session->step = (session->observed_addr == session->desired_addr)
                                ? (uint8_t)STEP_SET_MODE
                                : (uint8_t)STEP_SET_ADDR;
            start_step(session, now_ms);
            return;
        }

        if (config_matches(session))
        {
            LOG_INFO("session: configured");
            session->state = SESSION_READY;
        }
        else
        {
            LOG_ERROR("session: configuration mismatch");
            session->state = SESSION_FAILED;
        }
        return;
    }

    if (evt->status != PROTO_STATUS_OK)
    {
        LOG_WARNING("session: status 0x%02X on step %u", (unsigned)evt->status,
                    session->step);
        session->step = (uint8_t)STEP_VERIFY;
        start_step(session, now_ms);
        return;
    }

    next_step(session, now_ms);
}

bool session_active(const session_t *session)
{
    return (session != NULL) && (session->state == SESSION_WAIT_REPLY);
}

bool session_ready(const session_t *session)
{
    return (session != NULL) && (session->state == SESSION_READY);
}

void session_abort(session_t *session)
{
    if (session == NULL)
        return;
    if (session->state == SESSION_IDLE)
        return;

    LOG_WARNING("session: aborted in state %d", (int)session->state);
    session->state = SESSION_IDLE;
}
//<<----------------------