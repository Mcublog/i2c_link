/**
 * @file buffer.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2025-12-07
 *
 * @copyright mcublog Copyright (c) 2025
 *
 */
#include "buffer.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME dbgp
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

/**
 * @brief
 *
 * @param buffer
 * @param size
 */
void dbg_print_buffer(const uint8_t *buffer, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        LOG_RAW_INFO("%02X ", buffer[i]);
        if (i && i % 20 == 0)
            LOG_RAW_INFO("\n");
    }
     LOG_RAW_INFO("\r\n");
}