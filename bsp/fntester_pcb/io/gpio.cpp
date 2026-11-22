/**
 * @file gpio.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief GPIO для платы fntester_pcb (STM32F072)
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include "app/io/ll/igpio.h"

#include "main.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME gpio
#define LOG_MODULE_LEVEL (2)
#include "libs/debug/log_libs.h"
//<<----------------------

//>>---------------------- Public
void io_gpio_init(void)
{
#if !defined(LED_GREEN_Pin) || !defined(LED_GREEN_GPIO_Port)
    // NOTE: метки LED_GREEN_Pin/LED_GREEN_GPIO_Port не заданы в CubeMX
#endif
}

void io_gpio_led1(bool on)
{
#if defined(LED_GREEN_Pin) && defined(LED_GREEN_GPIO_Port)
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    (void)on;
#endif
}
//<<----------------------