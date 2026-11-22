/**
 * @file delay.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-23
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "app/utils/idelay.h"

#include "main.h"

void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}