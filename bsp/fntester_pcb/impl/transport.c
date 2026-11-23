/**
 * @file transport.c
 * @brief USB VCP транспорт для платы fntester_pcb
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include "app/io/itransport.h"

#include <string.h>

#include "hw/usb_vcp.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME trans
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Размер буфера передачи */
#define TRANSPORT_TX_MAX (256)

/** @brief Обработчик приёма приложения */
static transport_rx_handler_t m_handler = NULL;
/** @brief Буфер передачи (данные живут до завершения USB-передачи) */
static uint8_t m_tx_buf[TRANSPORT_TX_MAX];
//<<----------------------

//>>---------------------- Private
/**
 * @brief Принять данные из USB VCP (контекст прерывания)
 *
 * @param chunk принятый фрагмент
 * @return uint32_t количество обработанных байт
 */
static uint32_t usb_rx_handler(ios_chunk_t *chunk)
{
    if ((chunk == NULL) || (chunk->data == NULL) || (chunk->size == 0))
        return 0;
    if (m_handler != NULL)
        m_handler(chunk->data, chunk->size);
    return chunk->size;
}
//<<----------------------

//>>---------------------- Public
bool transport_init(transport_rx_handler_t handler)
{
    m_handler = handler;
    return usb_vcp_init(usb_rx_handler);
}

bool transport_write(const uint8_t *data, uint32_t size)
{
    if ((data == NULL) || (size == 0) || (size > sizeof(m_tx_buf)))
        return false;

    // NOTE: CDC_Transmit_FS использует буфер до конца передачи, поэтому
    // новую порцию можно копировать только когда передатчик свободен
    if (!usb_vcp_tx_ready())
        return false;

    memcpy(m_tx_buf, data, size);
    return usb_vcp_write(m_tx_buf, size);
}
//<<----------------------