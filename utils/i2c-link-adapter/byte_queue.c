/**
 * @file byte_queue.c
 * @brief Очередь байтов для буферов передачи адаптера
 * @version 0.1
 * @date 2026-11-26
 *
 */
#include "byte_queue.h"

#include <string.h>

//>>---------------------- Public
void byte_queue_init(byte_queue_t *queue, uint8_t *storage, uint32_t capacity)
{
    if ((queue == NULL) || (storage == NULL))
        return;

    queue->storage = storage;
    queue->capacity = capacity;
    queue->dropped = 0;

    ring_buffer_init(&queue->rb, (char *)storage, capacity);
}

bool byte_queue_push(byte_queue_t *queue, const uint8_t *data, uint32_t size)
{
    if ((queue == NULL) || (data == NULL) || (size == 0))
        return false;

    uint32_t used = (uint32_t)ring_buffer_num_items(&queue->rb);

    // NOTE: кладём только целую порцию, иначе разорвём поток байтов
    if ((used + size) >= queue->capacity)
    {
        queue->dropped++;
        return false;
    }

    ring_buffer_queue_arr(&queue->rb, (const char *)data,
                          (ring_buffer_size_t)size);
    return true;
}

uint32_t byte_queue_pop(byte_queue_t *queue, uint8_t *out, uint32_t cap)
{
    if ((queue == NULL) || (out == NULL) || (cap == 0))
        return 0;

    ring_buffer_size_t size = ring_buffer_dequeue_arr(
        &queue->rb, (char *)out, (ring_buffer_size_t)cap);

    return (uint32_t)size;
}

uint32_t byte_queue_used(const byte_queue_t *queue)
{
    if (queue == NULL)
        return 0;

    return (uint32_t)ring_buffer_num_items(&queue->rb);
}

bool byte_queue_empty(const byte_queue_t *queue)
{
    return byte_queue_used(queue) == 0;
}

void byte_queue_clear(byte_queue_t *queue)
{
    if (queue == NULL)
        return;

    ring_buffer_init(&queue->rb, (char *)queue->storage, queue->capacity);
}
//<<----------------------