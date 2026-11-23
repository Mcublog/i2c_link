/**
 * @file i2c.c
 * @brief Эмуляция шины I2C на хосте (desktop)
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include "app/i2c/ii2c.h"

#include <string.h>
//>>---------------------- Log control
#define LOG_MODULE_NAME i2cemu
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Количество эмулируемых устройств */
#define EMU_DEVICE_COUNT (2)
/** @brief Размер регистрового файла устройства */
#define EMU_REG_COUNT (256)
/** @brief Скорость эмулируемой шины, кГц */
#define EMU_SPEED_KHZ (100)

/** @brief Адреса эмулируемых устройств */
static const uint8_t kAddresses[EMU_DEVICE_COUNT] = {0x0C, 0x50};
/** @brief Регистровые файлы эмулируемых устройств */
static uint8_t m_regs[EMU_DEVICE_COUNT][EMU_REG_COUNT];
//<<----------------------

//>>---------------------- Private
/**
 * @brief Найти индекс эмулируемого устройства по адресу
 *
 * @param addr 7-битный адрес
 * @return int индекс или -1, если устройства нет
 */
static int device_index(uint8_t addr)
{
    for (int i = 0; i < EMU_DEVICE_COUNT; ++i)
    {
        if (kAddresses[i] == addr)
            return i;
    }
    return -1;
}
//<<----------------------

//>>---------------------- Public
bool i2c_init(void)
{
    memset(m_regs, 0, sizeof(m_regs));
    LOG_INFO("desktop i2c emulation: %d devices", EMU_DEVICE_COUNT);
    return true;
}

i2c_status_t i2c_probe(uint8_t addr)
{
    if (addr > 0x7F)
        return I2C_STATUS_PARAM;
    return (device_index(addr) >= 0) ? I2C_STATUS_OK : I2C_STATUS_NACK;
}

i2c_status_t i2c_read(uint8_t addr, uint16_t reg, uint8_t reg_size,
                      uint8_t *data, uint16_t len)
{
    (void)reg_size;

    if ((addr > 0x7F) || (reg_size > 2))
        return I2C_STATUS_PARAM;
    if ((len != 0) && (data == NULL))
        return I2C_STATUS_PARAM;

    int index = device_index(addr);
    if (index < 0)
        return I2C_STATUS_NACK;
    if (len == 0)
        return I2C_STATUS_OK;

    for (uint16_t i = 0; i < len; ++i)
        data[i] = m_regs[index][(reg + i) & (EMU_REG_COUNT - 1)];

    return I2C_STATUS_OK;
}

i2c_status_t i2c_write(uint8_t addr, uint16_t reg, uint8_t reg_size,
                       const uint8_t *data, uint16_t len)
{
    (void)reg_size;

    if ((addr > 0x7F) || (reg_size > 2))
        return I2C_STATUS_PARAM;
    if ((len != 0) && (data == NULL))
        return I2C_STATUS_PARAM;

    int index = device_index(addr);
    if (index < 0)
        return I2C_STATUS_NACK;
    if (len == 0)
        return I2C_STATUS_OK;

    for (uint16_t i = 0; i < len; ++i)
        m_regs[index][(reg + i) & (EMU_REG_COUNT - 1)] = data[i];

    return I2C_STATUS_OK;
}

i2c_status_t i2c_scan(uint8_t from, uint8_t to,
                      uint8_t bitmap[I2C_SCAN_BITMAP_SIZE])
{
    if ((bitmap == NULL) || (from > to) || (to > 0x7F))
        return I2C_STATUS_PARAM;

    memset(bitmap, 0, I2C_SCAN_BITMAP_SIZE);

    for (int i = 0; i < EMU_DEVICE_COUNT; ++i)
    {
        uint8_t addr = kAddresses[i];
        if ((addr >= from) && (addr <= to))
            bitmap[addr >> 3] |= (uint8_t)(1u << (addr & 0x07));
    }

    return I2C_STATUS_OK;
}

i2c_status_t i2c_recover(void)
{
    return I2C_STATUS_OK;
}

uint32_t i2c_speed_khz(void)
{
    return EMU_SPEED_KHZ;
}
//<<----------------------