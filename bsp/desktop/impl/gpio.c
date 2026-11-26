/**
 * @file gpio.c
 * @brief Хостовая реализация GPIO (заглушка/пример)
 * @version 0.1
 * @date 2026-09-01
 *
 */
#include <stdbool.h>
#include <stdint.h>

#include "app/io/ll/igpio.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME egpio
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Public
void io_gpio_init(void)
{
    LOG_INFO("gpio init (desktop stub)");
}

// NOTE: weak, чтобы юнит-тесты могли подменить и проверять состояние LED
__attribute__((weak)) void io_gpio_led1(bool on)
{
    LOG_DEBUG("led1: %s", on ? "on" : "off");
}
//<<----------------------
