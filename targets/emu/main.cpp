/**
 * @file main.cpp
 * @brief Эмулятор прошивки на хосте (desktop)
 * @version 0.1
 * @date 2026-09-01
 *
 */
#include "app/application.h"
#include "app/io/ll/igpio.h"
#include "app/storage/storage.h"
#include "app/system/systick.h"
#include "app/system/systime.h"
#include "app/utils/itimer.h"

#include "libs/SoftwareTimer/SoftTimers.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME emain
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    const stimer_init_ctx_t kInitCtx = {.enable_irq = app_timer_irq_enable,
                                        .disable_irq = app_timer_irq_disable};

    app_timer_init(systick_handler);
    io_gpio_init();
    systime_init();

    Timer_Init(&kInitCtx);

    application();

    return 0;
}
