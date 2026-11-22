/**
 * @file critical.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2025-12-07
 *
 * @copyright mcublog Copyright (c) 2025
 *
 */
#include <mutex>

#include "app/system/icritical.h"

static std::mutex m_mtx = {};

void critical_enter(void)
{
    m_mtx.lock();
}

void critical_exit(void)
{
    m_mtx.unlock();
}
