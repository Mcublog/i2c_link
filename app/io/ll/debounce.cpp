/**
 * @file debounce.cpp
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief
 * @version 0.1
 * @date 2025-12-07
 *
 * @copyright mcublog Copyright (c) 2025
 *
 */
#include <cassert>
#include <cstring>
#include <bit>

#include "debounce.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME dbnc
#if defined(NDEBUG)
#define LOG_MODULE_LEVEL (0)
#else
#define LOG_MODULE_LEVEL (3)
#endif
#include "log_libs.h"
//<<----------------------

//>>---------------------- Locals
static uint32_t one_number(const uint8_t *buffer, uint32_t size)
{
    uint32_t bits = 0;
    for (uint32_t i = 0; i < size; i++)
    {
        bits += buffer[i];
    }
    return bits;
}

static bool all_number_counter(const uint8_t *buffer, uint32_t size, uint8_t number)
{
    uint32_t bits = 0;
    for (uint32_t i = 0; i < size; i++)
    {
        bits += buffer[i];
    }
    if (number == 0)
        return bits == 0;
    return bits == size;
}

static bool one_counter(const uint8_t *buffer, uint32_t size)
{
    return all_number_counter(buffer, size, 1);
}

static bool is_power_of_two(int n)
{
    if (n % 2 != 0)
        return false;
    return n > 0 && (n & (n - 1)) == 0;
}
//<<----------------------

bool dbnc_init(debouncer_t *db, bool state)
{
    return dbnc_init_with_size(db, state, DEBOUNCE_SIZE);
}

bool dbnc_init_with_size(debouncer_t *db, bool state, uint8_t size)
{
    bool res = true;
    uint8_t actual_size = size;
    if (size == 1)
    {
        // режим работ
        LOG_WARNING("debounce off");
        actual_size = DEBOUNCE_SIZE;
    }
    else if (actual_size == 0 || actual_size > DEBOUNCE_SIZE || !is_power_of_two(actual_size))
    {
        LOG_WARNING("size invalid, using DEBOUNCE_SIZE");
        size = DEBOUNCE_SIZE;
        actual_size = DEBOUNCE_SIZE;
        res = false;
    }
    db->actual_size = size;
    db->state_counter = 0;
    db->last_state = state;
    ring_buffer_init(&db->rb, reinterpret_cast<char *>(db->buffer), actual_size);
    while (!dbnc_is_ready(db))
    {
        dbnc_set_current_state(db, state);
    }
    dbnc_set_current_state(db, state);

    LOG_DEBUG("initial state: %d", dbnc_get_state(db));
    return res;
}

void dbnc_set_current_state(debouncer_t *db, bool state)
{
    if (db->actual_size == 1)
        db->last_state = state;
    ring_buffer_queue(&db->rb, state);
}

bool dbnc_get_state(const debouncer_t *db)
{
    if (db->actual_size == 1)
    {
        return db->last_state;
    }
    return one_counter(db->buffer, db->actual_size);
}

uint8_t dbnc_get_actual_size(const debouncer_t *db)
{
    return db->actual_size;
}

egde_t dbnc_get_edge(debouncer_t *db)
{
    if (db->actual_size == 1)
    {
        LOG_ERROR("edge not working when debounce off");
        return egde_t::EDGE_LOW_PLATO;
    }

    const uint32_t kOnes = one_number(db->buffer, db->actual_size);
    if (kOnes == db->actual_size)
    {
        if (db->last_state != true)
        {
            db->last_state = true;
            return EDGE_RAISING;
        }
        db->state_counter++;
        return EDGE_HIGH_PLATO;
    }
    else if (kOnes == 0)
    {
        if (db->last_state != false)
        {
            db->last_state = false;
            return EDGE_FALLING;
        }
        db->state_counter++;
        return EDGE_LOW_PLATO;
    }

    db->state_counter = 0;
    return EDGE_BOUNCE;
}

bool dbnc_is_ready(const debouncer_t *db)
{
    if (db->actual_size == 1 && ring_buffer_num_items(&db->rb) >= 1)
        return true;

    return ring_buffer_is_full(&db->rb);
}

uint16_t dgnc_state_counter(const debouncer_t *db)
{
    return db->state_counter;
}

void dgnc_reset_state_counter(debouncer_t *db)
{
    db->state_counter = 0;
}
