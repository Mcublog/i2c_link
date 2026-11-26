/**
 * @file activity.c
 * @brief Индикатор активности шины I2C на LED1
 * @version 0.1
 * @date 2026-11-27
 *
 */
#include "app/io/activity.h"

#include <stdbool.h>
#include <stdint.h>

#include "app/io/ll/igpio.h"
#include "app/system/systick.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME activity
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Признак идущей транзакции */
static volatile bool m_busy;
/** @brief Момент, после которого LED можно погасить, мс */
static volatile uint32_t m_off_at;
/** @brief Текущее состояние LED1 */
static volatile bool m_lit;
//<<----------------------

//>>---------------------- Public
void activity_init(void)
{
    m_busy = false;
    m_off_at = 0;
    m_lit = false;
    io_gpio_led1(false);
}

void activity_begin(void)
{
    m_busy = true;
    m_off_at = (uint32_t)systick() + ACTIVITY_HOLD_MS;
    if (!m_lit)
    {
        m_lit = true;
        io_gpio_led1(true);
    }
}

void activity_end(void)
{
    m_off_at = (uint32_t)systick() + ACTIVITY_HOLD_MS;
    m_busy = false;
}

void activity_poll(void)
{
    uint32_t now = (uint32_t)systick();

    if (m_busy)
    {
        if (!m_lit)
        {
            m_lit = true;
            io_gpio_led1(true);
        }
        return;
    }

    // NOTE: повторная проверка m_busy сужает окно гонки с activity_begin()
    // из ISR: иначе LED мог погаснуть в самом начале транзакции
    if (m_lit && !m_busy && ((int32_t)(now - m_off_at) >= 0))
    {
        m_lit = false;
        io_gpio_led1(false);
    }
}
//<<----------------------
