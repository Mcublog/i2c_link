/**
 * @file time.cpp
 * @brief Хостовая реализация времени (заглушка/пример)
 * @version 0.1
 * @date 2026-09-01
 *
 */
#include "app/system/systime.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME systime
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Public
void systime_init(void)
{
    LOG_INFO("systime_init (desktop stub)");
}
//<<----------------------