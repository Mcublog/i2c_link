/**
 * @file delay.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Задержки для STM32F072
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "app/utils/idelay.h"
#include "app/utils/itick_timer.h"

#include "main.h"
#include "tim.h"

//>>---------------------- Public
void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

void delay_us(uint16_t us)
{
    // NOTE: TIM6 настроен в CubeMX на 1 МГц (prescaler = 47 при 48 МГц)
    tt_delay_us(&htim6, us);
}
//<<----------------------