/**
 * @file uart.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief UART2 transport for FN protocol (ws407)
 * @version 0.1
 * @date 2026-08-09
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "hw/uart.h"

#include "main.h"
#include "usart.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME huart
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------
//>>---------------------- Local
static uart2_rx_callback_t m_uart2_rx_clb = NULL;
static uint8_t m_rx_byte = 0;
//<<----------------------

//>>---------------------- Private
//<<----------------------

//>>---------------------- Public
void uart2_init(uart2_rx_callback_t clb)
{
    m_uart2_rx_clb = clb;
    HAL_UART_Receive_IT(&huart2, &m_rx_byte, 1);
}

int uart2_send_data(const uint8_t *data, size_t s)
{
    if (HAL_UART_Transmit(&huart2, (uint8_t *)data, s, 1000) != HAL_OK)
    {
        LOG_ERROR("uart2 tx error");
        return -1;
    }
    return 0;
}

void uart2_irq_handler(void)
{
    HAL_UART_IRQHandler(&huart2);
}
//<<----------------------

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (m_uart2_rx_clb)
            m_uart2_rx_clb(m_rx_byte);
        HAL_UART_Receive_IT(&huart2, &m_rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        LOG_WARNING("uart2 rx error: 0x%x", huart->ErrorCode);
        HAL_UART_Receive_IT(&huart2, &m_rx_byte, 1);
    }
}
