/**
 * @file critical.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2026-07-03
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include <stdint.h>

#include "app/system/icritical.h"

#include "targets/ws407/Drivers/CMSIS/Include/cmsis_gcc.h"

void critical_enter(void)
{
    __disable_irq();
}

void critical_exit(void)
{
    __enable_irq();
}
