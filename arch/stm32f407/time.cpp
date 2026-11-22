/**
 * @file ll_rtc.c
 * @author Viacheslav (v.konovalov@crystals.ru)
 * @brief
 * @version 0.1
 * @date 2024-07-02
 *
 * @copyright CSI Copyright (c) 2024
 *
 */
#include <stdbool.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>

#include "app/system/systime.h"

#include "main.h"
#include "rtc.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME hwtime
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------
//>>---------------------- Private

// ---------------------- Definitions --------------------------
#define TIME_ON_RESET_STATE (946684800U)
#define TIME_FOR_CHECK_SYNC (1577836800U) // 01/01/2020 @ 00:00
//--------------------------------------------------------------
static bool rtc_is_ready(void)
{
    return (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == SET);
}

static void rtc_init(void)
{
    if (rtc_is_ready())
        return;
    MX_RTC_Init();
}

static time_t get_current_time(void)
{
    RTC_TimeTypeDef rtc_time = {};
    RTC_DateTypeDef rtc_date = {};

    HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);

    struct tm time = {
            .tm_sec = rtc_time.Seconds,               // seconds [0,59]
            .tm_min = rtc_time.Minutes,               // min [0,59]
            .tm_hour = rtc_time.Hours,                // hour [0,23]
            .tm_mday = rtc_date.Date,                 // mday [1,31]
            .tm_mon = rtc_date.Month - 1,             // month [0,11]
            .tm_year = (rtc_date.Year + 2000) - 1900, // year (1900 == 0)
            .tm_wday = rtc_date.WeekDay - 1,          // week day (sunday = 0) [0,6]
            .tm_yday = 0,                                 // year day (1 january = 0) [0,365]
            .tm_isdst = 0,                                // flag Summer time (>0 - off)
        };
    return mktime(&time);
}

static void set_current_time(time_t t)
{
    struct tm *time = localtime(&t);
    RTC_TimeTypeDef rtc_time = {};

    rtc_time.Hours = (uint8_t)time->tm_hour;
    rtc_time.Minutes = (uint8_t)time->tm_min;
    rtc_time.Seconds = (uint8_t)time->tm_sec;

    RTC_DateTypeDef rtc_date = {};

    rtc_date.Month = (uint8_t)time->tm_mon + 1;
    rtc_date.Date  = (uint8_t)time->tm_mday;
    rtc_date.Year  = (uint8_t)time->tm_year - (2000 - 1900);

    if (rtc_is_ready() == false)
        rtc_init();

    HAL_RTC_SetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);
}
//<<----------------------

//>>---------------------- Public
void systime_init(void)
{
    LOG_INFO("systime_init");
}

/**
 * @brief
 *
 * @param tv
 * @param tz
 * @return int
 */
__attribute__((__used__))
int settimeofday(const struct timeval *tv, const struct timezone *tz)
{
    UNUSED(tz);
    set_current_time((time_t)tv->tv_sec);
    return 0;
}

/**
 * @brief
 *
 * @param r
 * @param t
 * @return clock_t
 */
__attribute__((__used__))
clock_t _times_r(struct _reent *r, struct tms *t)
{
    clock_t tick = HAL_GetTick();
    t->tms_utime = tick; /* User CPU time.  */
    t->tms_stime = tick; /* System CPU time.  */

    t->tms_cutime = tick; /* User CPU time of dead children.  */
    t->tms_cstime = tick; /* System CPU time of dead children.  */
    return tick;
}

/**
 * @brief
 *
 * @param arg
 * @return time_t
 */
[[gnu::used]]
time_t time( time_t *arg )
{
    time_t t = get_current_time();
    if (arg)
        *arg = t;
    return t;
}

/**
 * @brief
 *
 * @param tv
 * @param tzvp
 * @return int
 */
__attribute__((__used__))
int _gettimeofday(struct timeval *tv, void *tzvp)
{
    UNUSED(tzvp);
    if (rtc_is_ready() == false)
        rtc_init();

    tv->tv_usec = 0;
    tv->tv_sec = get_current_time();
    return 0;
}
//<<----------------------