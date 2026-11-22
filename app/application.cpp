/**
 * @file application.cpp
 * @brief Шаблонное приложение — платформонезависимая логика
 * @version 0.1
 * @date 2026-09-01
 *
 */
#include <ctime>

#include "application.h"

#include "app/io/ll/igpio.h"
#include "app/storage/storage.h"
#include "app/system/systick.h"
#include "app/system/systime.h"
#include "app/utils/idelay.h"
#include "app/version.h"

#include "libs/SoftwareTimer/SoftTimers.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME app
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
#define TIMER_1S_PERIOD (1000)
//<<----------------------

//>>---------------------- Private
static void every_second(uint8_t id)
{
    LOG_INFO("tick: %llu s", (unsigned long long)systick());
    Timer_Start(id, TIMER_1S_PERIOD);
}
//<<----------------------

//>>---------------------- Public
void application()
{
    LOG_INFO("fw: %s", FW_VERSION);

    systime_init();
    storage_init();
    io_gpio_init();

    uint8_t id = Timer_Create((timer_handler_t)every_second);
    Timer_Start(id, TIMER_1S_PERIOD);

    LOG_INFO("application started");

    while (1)
    {
        if (systime_is_need_to_init())
            systime_sync(time(nullptr));

        delay_ms(1);
    }
}
//<<----------------------
