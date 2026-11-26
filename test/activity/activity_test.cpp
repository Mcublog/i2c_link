/**
 * @file activity_test.cpp
 * @brief Юнит-тесты индикатора активности шины I2C (LED1)
 * @version 0.1
 * @date 2026-11-27
 *
 */
#include <cstdint>

#include "app/io/activity.h"
#include "app/io/ll/igpio.h"
#include "app/system/systick.h"
#include "libs/SoftwareTimer/SoftTimers.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME utest
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Счётчик неудачных проверок */
static int m_failures;

/**
 * @brief Проверка условия
 */
#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            LOG_ERROR("check failed: %s:%d: %s", __FILE__, __LINE__, #cond); \
            m_failures++;                                                    \
        }                                                                    \
    } while (0)

/** @brief Текущее состояние LED1, полученное от мока */
static bool m_led_on;
/** @brief Число обращений к LED1 */
static int m_led_writes;

/**
 * @brief Мок управления LED1 (перекрывает weak-заглушку desktop-слоя)
 *
 * @param on требуемое состояние
 */
extern "C" void io_gpio_led1(bool on)
{
    m_led_on = on;
    m_led_writes++;
}
//<<----------------------

//>>---------------------- Private
/**
 * @brief Разрешить прерывания таймера (заглушка)
 */
static void irq_enable(void)
{
}

/**
 * @brief Запретить прерывания таймера (заглушка)
 */
static void irq_disable(void)
{
}

/**
 * @brief Продвинуть системное время на @p ms миллисекунд
 *
 * @param ms число миллисекунд
 */
static void tick_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; ++i)
        systick_handler();
}

/**
 * @brief Подготовить таймеры и индикатор к тесту
 */
static void prepare(void)
{
    const stimer_init_ctx_t kInitCtx = {.enable_irq = irq_enable,
                                        .disable_irq = irq_disable};

    Timer_Init(&kInitCtx);
    activity_init();
    m_led_writes = 0;
}

/**
 * @brief begin включает LED, end гасит его через выдержку
 */
static void test_begin_on_and_hold(void)
{
    prepare();

    activity_begin();
    CHECK(m_led_on);

    tick_ms(5);
    activity_end();
    CHECK(m_led_on);

    tick_ms(ACTIVITY_HOLD_MS - 1);
    activity_poll();
    CHECK(m_led_on);

    tick_ms(1);
    activity_poll();
    CHECK(!m_led_on);
}

/**
 * @brief Идущая транзакция удерживает LED дольше выдержки
 */
static void test_busy_keeps_on(void)
{
    prepare();

    activity_begin();
    tick_ms(ACTIVITY_HOLD_MS * 2);
    activity_poll();
    CHECK(m_led_on);

    activity_end();
    activity_poll();
    CHECK(m_led_on);

    tick_ms(ACTIVITY_HOLD_MS);
    activity_poll();
    CHECK(!m_led_on);
}

/**
 * @brief В покое poll не дёргает GPIO повторно
 */
static void test_no_spurious_writes(void)
{
    prepare();

    const int writes = m_led_writes;

    for (int i = 0; i < 5; ++i)
        activity_poll();

    CHECK(!m_led_on);
    CHECK(m_led_writes == writes);
}

/**
 * @brief init принудительно гасит LED
 */
static void test_init_turns_off(void)
{
    prepare();

    activity_begin();
    CHECK(m_led_on);

    activity_init();
    CHECK(!m_led_on);
}
//<<----------------------

//>>---------------------- Public
int main()
{
    test_begin_on_and_hold();
    test_busy_keeps_on();
    test_no_spurious_writes();
    test_init_turns_off();

    if (m_failures != 0)
    {
        LOG_ERROR("activity tests failed: %d checks", m_failures);
        return -1;
    }

    LOG_INFO("activity tests passed");
    return 0;
}
//<<----------------------
