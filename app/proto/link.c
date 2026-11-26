/**
 * @file link.c
 * @brief Канальный уровень протокола host <-> I2C (COBS/R + CRC-16)
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include "app/proto/link.h"

#include <string.h>

#include "app/proto/frame.h"
#include "cobsr.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME proto
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Признак терминатора кадра */
#define PROTO_DELIMITER (0x00)
/** @brief Максимальная вместимость кольцевого буфера */
#define TX_RING_CAPACITY (PROTO_TX_RING_SIZE - 1)
//<<----------------------

//>>---------------------- Private
/**
 * @brief Отправить ответ в кольцевой буфер передачи
 *
 * @param link состояние
 * @param resp ответ
 */
static void queue_response(proto_link_t *link, const proto_resp_t *resp);

/**
 * @brief Обработать накопленный кадр
 *
 * @param link состояние
 */
static void process_frame(proto_link_t *link);

/**
 * @brief Попробовать отправить порцию из tx_stage
 *
 * @param link состояние
 * @return true порция отправлена
 */
static bool flush_stage(proto_link_t *link);
//<<----------------------

//>>---------------------- Private
static void queue_response(proto_link_t *link, const proto_resp_t *resp)
{
    uint8_t encoded[PROTO_MAX_COBS_FRAME];

    if (link->write == NULL)
    {
        link->stats.tx_drop++;
        return;
    }

    uint32_t raw_len = proto_frame_build_resp(link->tx_raw,
                                              sizeof(link->tx_raw), resp);
    if (raw_len == 0)
    {
        link->stats.tx_drop++;
        return;
    }

    cobsr_encode_result result = cobsr_encode(encoded, sizeof(encoded),
                                              link->tx_raw, raw_len);
    if (result.status != COBSR_ENCODE_OK)
    {
        link->stats.tx_drop++;
        return;
    }

    if ((ring_buffer_num_items(&link->tx_ring_rb) + result.out_len + 1) >
        TX_RING_CAPACITY)
    {
        link->stats.tx_drop++;
        return;
    }

    ring_buffer_queue_arr(&link->tx_ring_rb, (const char *)encoded,
                          (ring_buffer_size_t)result.out_len);
    ring_buffer_queue(&link->tx_ring_rb, (char)PROTO_DELIMITER);
}

static void process_frame(proto_link_t *link)
{
    proto_req_t req;
    proto_resp_t resp;

    cobsr_decode_result result =
        cobsr_decode(link->rx_raw, sizeof(link->rx_raw), link->rx_cobs,
                     link->rx_cobs_len);
    if (result.status != COBSR_DECODE_OK)
    {
        link->stats.rx_frame_err++;
        return;
    }

    // NOTE: кадр от устройства (бит направления в msg_id) — это ответ
    // или событие slave-режима, а не запрос
    if (link->event != NULL)
    {
        uint32_t size = (uint32_t)result.out_len;
        bool is_response =
            (size > 1) && ((link->rx_raw[1] & PROTO_MSG_DIR_RESPONSE) != 0);
        if (is_response)
        {
            proto_resp_t evt;
            proto_status_t status =
                proto_frame_parse_response(link->rx_raw, size, &evt);
            if (status != PROTO_STATUS_OK)
            {
                if (status == PROTO_STATUS_CRC)
                    link->stats.rx_crc_err++;
                else
                    link->stats.rx_frame_err++;
                return;
            }
            link->stats.rx_ok++;
            link->event(&evt, link->ctx);
            return;
        }
    }

    proto_status_t status = proto_frame_parse(link->rx_raw,
                                              (uint32_t)result.out_len, &req);
    if ((status != PROTO_STATUS_OK) && (status != PROTO_STATUS_PARAM) &&
        (status != PROTO_STATUS_UNKNOWN_CMD))
    {
        if (status == PROTO_STATUS_CRC)
            link->stats.rx_crc_err++;
        else
            link->stats.rx_frame_err++;
        return;
    }

    memset(&resp, 0, sizeof(resp));
    resp.cmd = req.cmd;
    resp.msg_id = req.msg_id;
    resp.flags = req.flags;
    resp.addr = req.addr;
    resp.status = status;

    if (status == PROTO_STATUS_OK)
    {
        link->stats.rx_ok++;
        if (link->handler != NULL)
        {
            link->handler(&req, &resp, link->ctx);
        }
        else
        {
            resp.status = PROTO_STATUS_NOT_SUPPORTED;
            resp.len = 0;
            resp.data = NULL;
        }
    }

    queue_response(link, &resp);
}

static bool flush_stage(proto_link_t *link)
{
    if (link->tx_stage_len == 0)
        return true;

    if (!link->write(link->tx_stage, link->tx_stage_len, link->ctx))
    {
        link->tx_retry++;
        if (link->tx_retry > PROTO_TX_RETRY_LIMIT)
        {
            LOG_WARNING("tx retry limit, frame dropped");
            link->stats.tx_drop++;
            link->tx_stage_len = 0;
            link->tx_retry = 0;
        }
        return false;
    }

    link->tx_stage_len = 0;
    link->tx_retry = 0;
    return true;
}
//<<----------------------

//>>---------------------- Public
void proto_link_init(proto_link_t *link, proto_write_fn write,
                     proto_handler_fn handler, void *ctx)
{
    memset(link, 0, sizeof(*link));
    link->write = write;
    link->handler = handler;
    link->ctx = ctx;
    ring_buffer_init(&link->tx_ring_rb, link->tx_ring,
                     sizeof(link->tx_ring));
}

void proto_link_set_event_handler(proto_link_t *link, proto_event_fn event)
{
    if (link == NULL)
        return;
    link->event = event;
}

void proto_link_feed(proto_link_t *link, const uint8_t *data, uint32_t size)
{
    if ((link == NULL) || (data == NULL))
        return;

    for (uint32_t i = 0; i < size; ++i)
    {
        uint8_t byte = data[i];

        if (byte != PROTO_DELIMITER)
        {
            if (link->rx_dropping)
                continue;
            if (link->rx_cobs_len < sizeof(link->rx_cobs))
            {
                link->rx_cobs[link->rx_cobs_len++] = byte;
            }
            else
            {
                link->stats.rx_overflow++;
                link->rx_dropping = true;
            }
            continue;
        }

        if (!link->rx_dropping && (link->rx_cobs_len != 0))
            process_frame(link);

        link->rx_cobs_len = 0;
        link->rx_dropping = false;
    }
}

void proto_link_poll(proto_link_t *link)
{
    if ((link == NULL) || (link->write == NULL))
        return;

    if (!flush_stage(link))
        return;

    while (!ring_buffer_is_empty(&link->tx_ring_rb))
    {
        ring_buffer_size_t size = ring_buffer_dequeue_arr(
            &link->tx_ring_rb, (char *)link->tx_stage,
            sizeof(link->tx_stage));
        if (size == 0)
            break;

        link->tx_stage_len = (uint32_t)size;
        if (!flush_stage(link))
            return;
    }
}

const proto_stats_t *proto_link_stats(const proto_link_t *link)
{
    if (link == NULL)
        return NULL;
    return &link->stats;
}

bool proto_link_send_frame(proto_link_t *link, const uint8_t *raw,
                           uint32_t size)
{
    uint8_t encoded[PROTO_MAX_COBS_FRAME];
    ring_buffer_size_t before;

    if ((link == NULL) || (raw == NULL))
        return false;
    if ((size == 0) || (size > PROTO_MAX_RAW_FRAME))
        return false;

    cobsr_encode_result result =
        cobsr_encode(encoded, sizeof(encoded), raw, size);
    if (result.status != COBSR_ENCODE_OK)
        return false;

    before = ring_buffer_num_items(&link->tx_ring_rb);
    if ((before + result.out_len + 1) > TX_RING_CAPACITY)
        return false;

    ring_buffer_queue_arr(&link->tx_ring_rb, (const char *)encoded,
                          (ring_buffer_size_t)result.out_len);
    ring_buffer_queue(&link->tx_ring_rb, (char)PROTO_DELIMITER);
    return true;
}

bool proto_link_send_event(proto_link_t *link, proto_cmd_t cmd, uint8_t msg_id,
                           uint8_t flags, uint8_t addr, proto_status_t status,
                           const uint8_t *data, uint8_t len)
{
    proto_resp_t event;
    ring_buffer_size_t before;

    if (link == NULL)
        return false;
    if (len > PROTO_MAX_DATA)
        return false;
    if ((len != 0) && (data == NULL))
        return false;

    memset(&event, 0, sizeof(event));
    event.cmd = cmd;
    event.msg_id = (uint8_t)(msg_id & PROTO_MSG_ID_MASK);
    event.flags = flags;
    event.addr = addr;
    event.len = len;
    event.status = status;
    event.data = data;

    before = ring_buffer_num_items(&link->tx_ring_rb);
    queue_response(link, &event);
    return ring_buffer_num_items(&link->tx_ring_rb) != before;
}
//<<----------------------