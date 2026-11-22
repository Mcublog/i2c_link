/**
 * @file systick.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2025-12-07
 *
 * @copyright mcublog Copyright (c) 2025
 *
 */
#include "systick.h"

#include "libs/SoftwareTimer/SoftTimers.h"

static volatile uint64_t m_tick_timer = 0;

void systick_handler(void)
{
    m_tick_timer = m_tick_timer + 1;
    Timer_Update();
}

uint64_t systick(void)
{
    return m_tick_timer;
}