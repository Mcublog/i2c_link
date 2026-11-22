/**
 * @file systime.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2026-08-08
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include <sys/time.h>

#include "systime.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME systime
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------
//>>---------------------- Private
#define TIME_FOR_CHECK_SYNC (1577836800U) // 01/01/2020 @ 00:00
//<<----------------------
/**
 * @brief
 *
 * @return true
 * @return false
 */
bool systime_is_need_to_init(void)
{
    if (time(nullptr) > TIME_FOR_CHECK_SYNC)
        return false;
    return true;
}

void systime_sync(time_t t)
{
    LOG_INFO("sync: %u", t);
    struct timeval tv = {};
    tv.tv_sec = (time_t)t;
    settimeofday(&tv, nullptr);
}
