/**
 * @file sandbox.cpp
 * @brief Песочница — место для экспериментов при разработке.
 * Собирается как отдельный исполняемый файл (dbg_sandbox), не входит в тесты.
 * @version 0.1
 * @date 2026-09-01
 *
 */
//>>---------------------- Log control
#define LOG_MODULE_NAME sndbox
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

int main()
{
    LOG_INFO("sandbox: hello");
    return 0;
}
