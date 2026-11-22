/**
 * @file critical.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Критические секции для STM32F072
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include <stdint.h>

#include "app/system/icritical.h"

#include "main.h"

void critical_enter(void)
{
    __disable_irq();
}

void critical_exit(void)
{
    __enable_irq();
}