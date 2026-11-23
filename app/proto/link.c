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
//<<----------------------