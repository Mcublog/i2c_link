/**
 * @file smoke_test.cpp
 * @brief Простейший (smoke) юнит-тест — проверяет, что проект собирается
 * и базовые проверки работают.
 * @version 0.1
 * @date 2026-09-01
 *
 */
#include <cstdint>

// Пример зависимости от платформонезависимого модуля приложения
#include "app/io/types.h"
#include "app/system/systick.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME smoke
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Private
/**
 * @brief Простейшая проверка
 *
 * @return int 0 - успешно, иначе ошибка
 */
static int check_basic_math(void)
{
    if ((1 + 1) != 2)
    {
        LOG_ERROR("basic math check failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Проверка, что enum из app/io/types.h корректен
 *
 * @return int 0 - успешно, иначе ошибка
 */
static int check_io_types(void)
{
    if (EDGE_LOW_PLATO != 0 || EDGE_HIGH_PLATO != 3)
    {
        LOG_ERROR("io types check failed");
        return -1;
    }
    return 0;
}
//<<----------------------

//>>---------------------- Public
int main()
{
    if (check_basic_math() != 0)
        return -1;

    if (check_io_types() != 0)
        return -1;

    LOG_INFO("smoke test passed");
    return 0;
}
//<<----------------------
