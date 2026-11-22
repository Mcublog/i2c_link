/**
 * @file gpio.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2025-12-07
 *
 * @copyright Viacheslav mcublog (c) 2025
 *
 */
#include "app/io/ll/igpio.h"

#include "main.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME gpio
#define LOG_MODULE_LEVEL (2)
#include "libs/debug/log_libs.h"
//<<----------------------

/**
 * @brief
 *
 * @param on
 */
void io_gpio_led1(bool on)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
//<<----------------------