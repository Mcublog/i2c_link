/**
 * @file uart.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief UART-транспорт на HAL для STM32F072
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "hw/uart.h"

#include "main.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME huart
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

#ifdef HAL_UART_MODULE_ENABLED
//>>---------------------- Local
#define UART_CONTEXTS_1010 (4U)

typedef struct
{
    UART_HandleTypeDef *huart; /**< экземпляр UART */
    uart_rx_callback_t clb;    /**< callback приёма */
    uint8_t byte;              /**< буфер приёма одного байта */
} uart_ctx_t;

static uart_ctx_t m_contexts[UART_CONTEXTS_1010];
//<<----------------------

//>>---------------------- Private
static uart_ctx_t *find_context(UART_HandleTypeDef *huart)
{
    for (uint32_t i = 0; i < UART_CONTEXTS_1010; ++i)
    {
        if (m_contexts[i].huart == huart)
            return &m_contexts[i];
    }
    return NULL;
}

static uart_ctx_t *alloc_context(UART_HandleTypeDef *huart)
{
    uart_ctx_t *ctx = find_context(huart);
    if (ctx)
        return ctx;

    for (uint32_t i = 0; i < UART_CONTEXTS_1010; ++i)
    {
        if (m_contexts[i].huart == NULL)
        {
            m_contexts[i].huart = huart;
            return &m_contexts[i];
        }
    }
    return NULL;
}
//<<----------------------

//>>---------------------- Public
void uart_init(UART_HandleTypeDef *huart, uart_rx_callback_t clb)
{
    if (!huart)
        return;

    uart_ctx_t *ctx = alloc_context(huart);
    if (!ctx)
    {
        LOG_ERROR("uart: no free context");
        return;
    }

    ctx->clb = clb;
    HAL_UART_Receive_IT(huart, &ctx->byte, 1);
}

int uart_send_data(UART_HandleTypeDef *huart, const uint8_t *data, size_t size)
{
    if (!huart || !data)
        return -1;

    if (HAL_UART_Transmit(huart, (uint8_t *)data, (uint16_t)size, 1000) !=
        HAL_OK)
    {
        LOG_ERROR("uart tx error");
        return -1;
    }
    return 0;
}

void uart_irq_handler(UART_HandleTypeDef *huart)
{
    HAL_UART_IRQHandler(huart);
}
//<<----------------------

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    uart_ctx_t *ctx = find_context(huart);
    if (!ctx)
        return;

    if (ctx->clb)
        ctx->clb(ctx->byte);
    HAL_UART_Receive_IT(huart, &ctx->byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    uart_ctx_t *ctx = find_context(huart);
    if (!ctx)
        return;

    LOG_WARNING("uart rx error: 0x%x", huart->ErrorCode);
    HAL_UART_Receive_IT(huart, &ctx->byte, 1);
}
#endif // HAL_UART_MODULE_ENABLED