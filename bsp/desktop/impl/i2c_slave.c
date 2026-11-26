/**
 * @file i2c_slave.c
 * @brief Заглушка slave-режима I2C для хоста (desktop)
 * @version 0.1
 * @date 2026-11-25
 *
 * Эмулятор не поддерживает slave-режим: внешнего мастера на виртуальной
 * шине нет. Функции помечены weak, чтобы юнит-тесты могли подменить
 * платформенный слой своими реализациями.
 */
#include "app/i2c/islave.h"

//>>---------------------- Public
__attribute__((weak)) bool i2c_slave_hw_start(uint8_t addr)
{
    (void)addr;
    return false;
}

__attribute__((weak)) void i2c_slave_hw_stop(void)
{
}

__attribute__((weak)) bool i2c_slave_hw_active(void)
{
    return false;
}

__attribute__((weak)) bool i2c_slave_hw_supply(const uint8_t *data,
                                               uint16_t len)
{
    (void)data;
    (void)len;
    return false;
}
//<<----------------------