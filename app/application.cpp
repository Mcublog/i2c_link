/**
 * @file application.cpp
 * @brief Приложение моста serial <-> I2C
 * @version 0.1
 * @date 2026-09-01
 *
 */
#include <ctime>

#include "app/application.h"

#include "app/io/activity.h"
#include "app/io/itransport.h"
#include "app/io/ll/igpio.h"
#include "app/i2c/i2c_service.h"
#include "app/proto/link.h"
#include "app/storage/storage.h"
#include "app/system/systick.h"
#include "app/system/systime.h"
#include "app/utils/idelay.h"
#include "app/version.h"

#include "libs/Ring-Buffer/ringbuffer.h"
#include "libs/SoftwareTimer/SoftTimers.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME app
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Период служебного таймера, мс */
#define TIMER_1S_PERIOD (1000)
/** @brief Размер буфера приёма (степень двойки) */
#define RX_RING_SIZE (256)
/** @brief Размер порции при разборе, байт */
#define RX_CHUNK_SIZE (64)

/** @brief Канальный уровень протокола */
static proto_link_t m_link;
/** @brief Кольцевой буфер приёма из транспорта */
static ring_buffer_t m_rx_ring;
/** @brief Память кольцевого буфера приёма */
static uint8_t m_rx_ring_buf[RX_RING_SIZE];
/** @brief Счётчик отброшенных порций при переполнении */
static uint32_t m_rx_ring_drop;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Служебный таймер: лог раз в секунду
 *
 * @param id идентификатор таймера
 */
static void every_second(uint8_t id)
{
    LOG_INFO("tick: %llu s", (unsigned long long)systick());
    Timer_Start(id, TIMER_1S_PERIOD);
}

/**
 * @brief Колбэк передачи протокола в транспорт
 *
 * @param data данные
 * @param size размер данных
 * @param ctx контекст
 * @return true - кадр принят транспортом
 */
static bool transport_send(const uint8_t *data, uint32_t size, void *ctx)
{
    (void)ctx;
    return transport_write(data, size);
}

/**
 * @brief Обработчик приёма из транспорта (ISR/поток чтения)
 *
 * @param data данные
 * @param size размер данных
 */
static void transport_receive(const uint8_t *data, uint32_t size)
{
    if ((data == NULL) || (size == 0))
        return;

    size_t items = ring_buffer_num_items(&m_rx_ring);
    if ((items + size) > (RX_RING_SIZE - 1))
    {
        m_rx_ring_drop++;
        return;
    }

    ring_buffer_queue_arr(&m_rx_ring, (const char *)data,
                          (ring_buffer_size_t)size);
}

/**
 * @brief Разобрать накопленные данные и вытолкнуть ответы
 */
static void process_transport(void)
{
    uint8_t chunk[RX_CHUNK_SIZE];

    for (;;)
    {
        ring_buffer_size_t size = ring_buffer_dequeue_arr(
            &m_rx_ring, (char *)chunk, sizeof(chunk));
        if (size == 0)
            break;
        proto_link_feed(&m_link, chunk, (uint32_t)size);
    }

    proto_link_poll(&m_link);
}
//<<----------------------

//>>---------------------- Public
void application()
{
    LOG_INFO("fw: %s", FW_VERSION);

    systime_init();
    storage_init();
    io_gpio_init();
    activity_init();

    ring_buffer_init(&m_rx_ring, (char *)m_rx_ring_buf,
                     sizeof(m_rx_ring_buf));
    proto_link_init(&m_link, transport_send, i2c_service_handle, &m_link);
    i2c_service_init(&m_link);

    if (!transport_init(transport_receive))
        LOG_ERROR("transport init failed");

    uint8_t id = Timer_Create((timer_handler_t)every_second);
    Timer_Start(id, TIMER_1S_PERIOD);

    LOG_INFO("application started");

    while (1)
    {
        transport_poll();
        i2c_service_poll();
        process_transport();
        activity_poll();

        if (systime_is_need_to_init())
            systime_sync(time(nullptr));

        delay_ms(1);
    }
}
//<<----------------------