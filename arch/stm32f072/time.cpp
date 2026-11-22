/**
 * @file time.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Время на базе HAL_GetTick для STM32F072 (без RTC)
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include <stdbool.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>

#include "app/system/systime.h"
#include "app/utils/macros.h"

#include "main.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME hwtime
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------
//>>---------------------- Local
// NOTE: опорное время по умолчанию (2026-01-01), чтобы systime_is_need_to_init()
//       не требовал синхронизации на каждом проходе до первого systime_sync()
#define TIME_DEFAULT_BASE_1010 (1767225600U)

static time_t m_time_base = (time_t)TIME_DEFAULT_BASE_1010;
static uint32_t m_tick_base = 0;
//<<----------------------

//>>---------------------- Private
static time_t current_time(void)
{
    const uint32_t elapsed_ms = HAL_GetTick() - m_tick_base;
    return m_time_base + (time_t)(elapsed_ms / 1000U);
}

static void set_current_time(time_t t)
{
    m_time_base = t;
    m_tick_base = HAL_GetTick();
}
//<<----------------------

//>>---------------------- Public
void systime_init(void)
{
    m_tick_base = HAL_GetTick();
    LOG_INFO("systime_init");
}

/**
 * @brief Установка системного времени
 *
 * @param tv новое время
 * @param tz не используется
 * @return int 0
 */
__attribute__((__used__))
int settimeofday(const struct timeval *tv, const struct timezone *tz)
{
    UNUSED(tz);
    set_current_time((time_t)tv->tv_sec);
    return 0;
}

/**
 * @brief Системное время в тиках для newlib
 *
 * @param r контекст reent
 * @param t буфер времён
 * @return clock_t тики HAL
 */
__attribute__((__used__))
clock_t _times_r(struct _reent *r, struct tms *t)
{
    UNUSED(r);
    const clock_t tick = (clock_t)HAL_GetTick();
    t->tms_utime = tick; /* User CPU time.  */
    t->tms_stime = tick; /* System CPU time.  */

    t->tms_cutime = tick; /* User CPU time of dead children.  */
    t->tms_cstime = tick; /* System CPU time of dead children.  */
    return tick;
}

/**
 * @brief Системное время
 *
 * @param arg если не NULL, туда также пишется результат
 * @return time_t текущее время
 */
[[gnu::used]]
time_t time(time_t *arg)
{
    const time_t t = current_time();
    if (arg)
        *arg = t;
    return t;
}

/**
 * @brief Системное время для newlib
 *
 * @param tv буфер времени
 * @param tzvp не используется
 * @return int 0
 */
__attribute__((__used__))
int _gettimeofday(struct timeval *tv, void *tzvp)
{
    UNUSED(tzvp);
    tv->tv_usec = 0;
    tv->tv_sec = current_time();
    return 0;
}
//<<----------------------