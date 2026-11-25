/**
 * @file transport.cpp
 * @brief Транспорт на хосте: UNIX-сокет через libs/serdev
 * @version 0.1
 * @date 2026-11-23
 *
 */
#include "app/io/itransport.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "Serial.hpp"
#include "usocket/USocketServDev.hpp"
//>>---------------------- Log control
#define LOG_MODULE_NAME trans
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Путь UNIX-сокета по умолчанию */
static const char *kDefaultEndpoint = "/tmp/i2c_link.sock";
/** @brief Переменная окружения с путём сокета */
static const char *kEndpointEnv = "I2C_LINK_EP";
/** @brief Обработчик приёма приложения */
static transport_rx_handler_t m_handler = nullptr;
/** @brief Текущий путь сокета */
static const char *m_endpoint = kDefaultEndpoint;
/** @brief Признак подключённого клиента */
static std::atomic<bool> m_connected = false;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Получить экземпляр устройства
 *
 * @return USocketServerDevice& устройство
 */
static USocketServerDevice &device_instance(void);

/**
 * @brief Поток чтения из сокета
 *
 * @param arg не используется
 * @return void* всегда nullptr
 */
static void *reader_thread(void *arg)
{
    (void)arg;

    USocketServerDevice &device = device_instance();

    while (true)
    {
        bool success = false;
        uint8_t byte = 0;

        // NOTE: USocketServerDevice::read блокирующий, поток спит до
        // прихода данных или разрыва соединения
        byte = device.read(&success);
        if (!success)
        {
            LOG_WARNING("client disconnected, waiting for a new one");
            m_connected = false;
            unlink(m_endpoint);
            device.helth();
            m_connected = true;
            continue;
        }

        if (m_handler != nullptr)
            m_handler(&byte, 1);
    }

    return nullptr;
}

/**
 * @brief Получить экземпляр устройства
 *
 * @return USocketServerDevice& устройство
 */
static USocketServerDevice &device_instance(void)
{
    static USocketServerDevice device(kDefaultEndpoint, reader_thread);
    return device;
}
//<<----------------------

//>>---------------------- Public
bool transport_init(transport_rx_handler_t handler)
{
    const char *endpoint = getenv(kEndpointEnv);
    if ((endpoint != nullptr) && (endpoint[0] != '\0'))
        m_endpoint = endpoint;

    m_handler = handler;

    static uint8_t in_buffer[256];
    static uint8_t out_buffer[256];
    ios_ctl_t ctl = {{in_buffer, sizeof(in_buffer)},
                     {out_buffer, sizeof(out_buffer)},
                     nullptr};

    unlink(m_endpoint);

    USocketServerDevice &device = device_instance();
    device.register_irq(nullptr);
    m_connected = false;
    bool result = device.init(m_endpoint, &ctl);
    if (!result)
    {
        LOG_ERROR("transport init failed");
        return false;
    }

    m_connected = true;
    LOG_INFO("waiting for clients on %s", m_endpoint);
    return true;
}

bool transport_write(const uint8_t *data, uint32_t size)
{
    if ((data == nullptr) || (size == 0))
        return false;
    if (!m_connected.load())
        return false;

    return device_instance().write(data, size);
}

void transport_poll(void)
{
    // NOTE: сокет-транспорт обслуживается отдельным потоком чтения
}
//<<----------------------