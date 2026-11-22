/**
 * @file timer.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "app/system/icritical.h"
#include "app/utils/itimer.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME timer
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Public
void app_timer_init(timer_systick_callback_t systick_callback)
{
}

void app_timer_irq_enable(void)
{
    critical_exit();
}

void app_timer_irq_disable(void)
{
    critical_enter();
}
//<<----------------------