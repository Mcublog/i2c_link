#ifndef APP_DEBUG_PRINT_BUFFER_H
#define APP_DEBUG_PRINT_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Вывести содержимое буфера в лог (hex-дамп)
     *
     * @param buffer буфер для вывода
     * @param size размер буфера
     */
    void dbg_print_buffer(const uint8_t *buffer, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // #define APP_DEBUG_PRINT_BUFFER_H