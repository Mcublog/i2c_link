/**
 * @file i2c.cpp
 * @brief Реализация шины I2C (I2C2) для платы fntester_pcb
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include "app/i2c/ii2c.h"

#include <string.h>

#include "app/utils/idelay.h"
#include "i2c.h"
#include "main.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME i2c
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Таймаут одной транзакции, мс */
#define I2C_TIMEOUT_MS (10)
/** @brief Таймаут пробы адреса при сканировании, мс */
#define I2C_SCAN_TIMEOUT_MS (2)
/** @brief Количество испытаний при пробе адреса */
#define I2C_PROBE_TRIALS (1)
/** @brief Полупериод импульса при восстановлении шины, мкс */
#define I2C_RECOVER_HALF_PERIOD_US (5)
/** @brief Количество импульсов SCL при восстановлении шины */
#define I2C_RECOVER_PULSES (9)
/** @brief Скорость шины, кГц */
#define I2C_SPEED_KHZ (100)
//<<----------------------

//>>---------------------- Private
/**
 * @brief Перевести статус HAL в статус шины
 *
 * @param status статус HAL
 * @return i2c_status_t статус шины
 */
static i2c_status_t map_hal_status(HAL_StatusTypeDef status);

/**
 * @brief Получить 8-битный адрес устройства для HAL
 *
 * @param addr 7-битный адрес
 * @return uint16_t адрес со сдвигом
 */
static uint16_t dev_address(uint8_t addr);

/**
 * @brief Передать только адрес регистра (без данных)
 *
 * @param addr 7-битный адрес устройства
 * @param reg адрес регистра
 * @param reg_size размер регистра
 * @return i2c_status_t
 */
static i2c_status_t write_register_only(uint8_t addr, uint16_t reg,
                                        uint8_t reg_size);
//<<----------------------

//>>---------------------- Private
static i2c_status_t map_hal_status(HAL_StatusTypeDef status)
{
    uint32_t error = HAL_I2C_GetError(&hi2c2);

    if (status == HAL_OK)
        return I2C_STATUS_OK;
    if (status == HAL_TIMEOUT)
        return I2C_STATUS_TIMEOUT;
    if (status == HAL_BUSY)
        return I2C_STATUS_BUSY;
    if (error & HAL_I2C_ERROR_AF)
        return I2C_STATUS_NACK;
    return I2C_STATUS_BUS;
}

static uint16_t dev_address(uint8_t addr)
{
    return (uint16_t)(addr << 1);
}

static i2c_status_t write_register_only(uint8_t addr, uint16_t reg,
                                        uint8_t reg_size)
{
    uint8_t buf[2];
    uint16_t size = 0;

    if (reg_size == 1)
    {
        buf[0] = (uint8_t)reg;
        size = 1;
    }
    else if (reg_size == 2)
    {
        buf[0] = (uint8_t)(reg & 0xFF);
        buf[1] = (uint8_t)(reg >> 8);
        size = 2;
    }
    else
    {
        return I2C_STATUS_PARAM;
    }

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        &hi2c2, dev_address(addr), buf, size, I2C_TIMEOUT_MS);
    return map_hal_status(status);
}
//<<----------------------

//>>---------------------- Public
bool i2c_init(void)
{
    // NOTE: I2C2 инициализируется CubeMX (MX_I2C2_Init) до запуска
    // приложения, здесь достаточно проверить состояние
    return HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_READY;
}

i2c_status_t i2c_probe(uint8_t addr)
{
    if (addr > 0x7F)
        return I2C_STATUS_PARAM;

    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(
        &hi2c2, dev_address(addr), I2C_PROBE_TRIALS, I2C_TIMEOUT_MS);
    if (status == HAL_OK)
        return I2C_STATUS_OK;
    return (status == HAL_BUSY) ? I2C_STATUS_BUSY : I2C_STATUS_NACK;
}

i2c_status_t i2c_read(uint8_t addr, uint16_t reg, uint8_t reg_size,
                      uint8_t *data, uint16_t len)
{
    if ((addr > 0x7F) || (reg_size > 2))
        return I2C_STATUS_PARAM;
    if ((len != 0) && (data == NULL))
        return I2C_STATUS_PARAM;
    if (len == 0)
        return i2c_probe(addr);

    HAL_StatusTypeDef status;

    if (reg_size == 0)
    {
        status = HAL_I2C_Master_Receive(&hi2c2, dev_address(addr), data, len,
                                        I2C_TIMEOUT_MS);
    }
    else
    {
        uint16_t mem_size = (reg_size == 1) ? I2C_MEMADD_SIZE_8BIT
                                            : I2C_MEMADD_SIZE_16BIT;
        status = HAL_I2C_Mem_Read(&hi2c2, dev_address(addr), reg, mem_size,
                                  data, len, I2C_TIMEOUT_MS);
    }

    return map_hal_status(status);
}

i2c_status_t i2c_write(uint8_t addr, uint16_t reg, uint8_t reg_size,
                       const uint8_t *data, uint16_t len)
{
    if ((addr > 0x7F) || (reg_size > 2))
        return I2C_STATUS_PARAM;
    if ((len != 0) && (data == NULL))
        return I2C_STATUS_PARAM;
    if ((len == 0) && (reg_size == 0))
        return i2c_probe(addr);
    if (len == 0)
        return write_register_only(addr, reg, reg_size);

    HAL_StatusTypeDef status;

    if (reg_size == 0)
    {
        status = HAL_I2C_Master_Transmit(&hi2c2, dev_address(addr),
                                         (uint8_t *)data, len,
                                         I2C_TIMEOUT_MS);
    }
    else
    {
        uint16_t mem_size = (reg_size == 1) ? I2C_MEMADD_SIZE_8BIT
                                            : I2C_MEMADD_SIZE_16BIT;
        status = HAL_I2C_Mem_Write(&hi2c2, dev_address(addr), reg, mem_size,
                                   (uint8_t *)data, len, I2C_TIMEOUT_MS);
    }

    return map_hal_status(status);
}

i2c_status_t i2c_scan(uint8_t from, uint8_t to,
                      uint8_t bitmap[I2C_SCAN_BITMAP_SIZE])
{
    if ((bitmap == NULL) || (from > to) || (to > 0x7F))
        return I2C_STATUS_PARAM;

    memset(bitmap, 0, I2C_SCAN_BITMAP_SIZE);

    for (uint16_t addr = from; addr <= to; ++addr)
    {
        HAL_StatusTypeDef status =
            HAL_I2C_IsDeviceReady(&hi2c2, dev_address((uint8_t)addr),
                                  I2C_PROBE_TRIALS, I2C_SCAN_TIMEOUT_MS);
        if (status == HAL_OK)
            bitmap[addr >> 3] |= (uint8_t)(1u << (addr & 0x07));
    }

    return I2C_STATUS_OK;
}

i2c_status_t i2c_recover(void)
{
    // NOTE: переводим линии в GPIO и тактируем SCL, пока ведомый не
    // отпустит SDA, затем формируем STOP и возвращаем I2C в работу
    HAL_I2C_DeInit(&hi2c2);

    GPIO_InitTypeDef gpio = {};
    gpio.Pin = FN_SCL_Pin | FN_SDA_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FN_SCL_GPIO_Port, &gpio);

    HAL_GPIO_WritePin(FN_SDA_GPIO_Port, FN_SDA_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FN_SCL_GPIO_Port, FN_SCL_Pin, GPIO_PIN_SET);
    delay_us(I2C_RECOVER_HALF_PERIOD_US);

    for (uint8_t i = 0; i < I2C_RECOVER_PULSES; ++i)
    {
        HAL_GPIO_WritePin(FN_SCL_GPIO_Port, FN_SCL_Pin, GPIO_PIN_RESET);
        delay_us(I2C_RECOVER_HALF_PERIOD_US);
        HAL_GPIO_WritePin(FN_SCL_GPIO_Port, FN_SCL_Pin, GPIO_PIN_SET);
        delay_us(I2C_RECOVER_HALF_PERIOD_US);
    }

    // STOP: SDA low -> SCL high -> SDA high
    HAL_GPIO_WritePin(FN_SCL_GPIO_Port, FN_SCL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FN_SDA_GPIO_Port, FN_SDA_Pin, GPIO_PIN_RESET);
    delay_us(I2C_RECOVER_HALF_PERIOD_US);
    HAL_GPIO_WritePin(FN_SCL_GPIO_Port, FN_SCL_Pin, GPIO_PIN_SET);
    delay_us(I2C_RECOVER_HALF_PERIOD_US);
    HAL_GPIO_WritePin(FN_SDA_GPIO_Port, FN_SDA_Pin, GPIO_PIN_SET);
    delay_us(I2C_RECOVER_HALF_PERIOD_US);

    bool released =
        (HAL_GPIO_ReadPin(FN_SDA_GPIO_Port, FN_SDA_Pin) == GPIO_PIN_SET) &&
        (HAL_GPIO_ReadPin(FN_SCL_GPIO_Port, FN_SCL_Pin) == GPIO_PIN_SET);

    MX_I2C2_Init();

    if (!released)
    {
        LOG_WARNING("i2c bus is still busy");
        return I2C_STATUS_BUS;
    }

    LOG_INFO("i2c bus recovered");
    return I2C_STATUS_OK;
}

uint32_t i2c_speed_khz(void)
{
    return I2C_SPEED_KHZ;
}
//<<----------------------