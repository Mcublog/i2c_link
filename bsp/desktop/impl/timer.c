/**
 * @file timer.c
 * @brief Хостовая реализация таймера (заглушка/пример)
 * @version 0.1
 * @date 2026-09-01
 *
 */
#include <stdbool.h>
#include <stdint.h>

#include "app/utils/itimer.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME timer
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Public
void app_timer_init(timer_systick_callback_t systick_callback)
{
    (void)systick_callback;
    LOG_INFO("timer init (desktop stub)");
}

void app_timer_irq_enable(void)
{
}

void app_timer_irq_disable(void)
{
}
//<<----------------------
