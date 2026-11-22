/**
 * @file tick_timer.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Микросекундный таймер (TIM6, 1 МГц) для STM32F072
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "app/utils/itick_timer.h"

#include "main.h"

//>>---------------------- Public
void tt_init(void *header)
{
    HAL_TIM_Base_Start((TIM_HandleTypeDef *)header);
}

void tt_delay_us(void *header, uint16_t us)
{
    tt_reset(header);
    while (tt_read(header) < us)
    {
    };
}

void tt_reset(void *header)
{
    ((TIM_HandleTypeDef *)header)->Instance->CNT = 0;
}

uint16_t tt_read(void *header)
{
    return (uint16_t)((TIM_HandleTypeDef *)header)->Instance->CNT;
}
//<<----------------------