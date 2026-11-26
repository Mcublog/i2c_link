/**
 * @file i2c_slave.cpp
 * @brief Реализация slave-режима I2C (I2C2) для платы fntester_pcb
 * @version 0.1
 * @date 2026-11-25
 *
 */
#include <string.h>

#include "app/i2c/islave.h"
#include "app/i2c/i2c_slave_service.h"
#include "app/io/activity.h"

#include "i2c.h"
#include "main.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME i2cslavehw
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Приёмный буфер одной транзакции */
static uint8_t m_rx_buf[PROTO_MAX_DATA];
/** @brief Передающий буфер одной транзакции */
static uint8_t m_tx_buf[PROTO_MAX_DATA];
/** @brief Признак активного slave-режима */
static bool m_active;
/** @brief Адрес, к которому обратился мастер */
static uint8_t m_addr;
/** @brief Число принятых в текущей транзакции байт */
static uint16_t m_write_len;
/** @brief Число переданных в текущей транзакции байт */
static uint16_t m_read_len;
/** @brief Длина подготовленного буфера передачи */
static uint16_t m_tx_len;
/** @brief Признак незавершённой транзакции */
static bool m_xfer_active;
/** @brief Фаза записи уже отправлена хосту */
static bool m_write_reported;
/** @brief Ожидание данных для фазы чтения (шина удерживается) */
static bool m_read_pending;
//<<----------------------

//>>---------------------- Private
/**
 * @brief Сбросить состояние текущей транзакции
 */
static void reset_xfer(void);

/**
 * @brief Завершить транзакцию и отправить событие SLAVE_DONE
 *
 * @param status результат транзакции
 */
static void finish_xfer(proto_status_t status);

/**
 * @brief Снова включить прослушивание адреса
 */
static void rearm(void);
//<<----------------------

//>>---------------------- Private
static void reset_xfer(void)
{
    m_write_len = 0;
    m_read_len = 0;
    m_tx_len = 0;
    m_xfer_active = false;
    m_write_reported = false;
    m_read_pending = false;
}

static void finish_xfer(proto_status_t status)
{
    if (!m_xfer_active)
    {
        rearm();
        return;
    }

    if (!m_write_reported && (m_write_len != 0))
        i2c_slave_service_on_write(m_addr, m_rx_buf, m_write_len, false);

    if (m_read_pending)
        status = PROTO_STATUS_TIMEOUT;

    i2c_slave_service_on_done(m_addr, m_write_len, m_read_len, status);
    activity_end();
    reset_xfer();
    rearm();
}

static void rearm(void)
{
    if (!m_active)
        return;
    if (hi2c2.State == HAL_I2C_STATE_READY)
    {
        if (HAL_I2C_EnableListen_IT(&hi2c2) != HAL_OK)
            LOG_WARNING("listen rearm failed");
    }
}
//<<----------------------

//>>---------------------- Public
bool i2c_slave_hw_start(uint8_t addr)
{
    if ((addr < I2C_SLAVE_ADDR_MIN) || (addr > I2C_SLAVE_ADDR_MAX))
        return false;

    if (m_active)
        i2c_slave_hw_stop();

    if (hi2c2.Init.Timing == 0)
        MX_I2C2_Init();

    reset_xfer();
    m_addr = addr;

    hi2c2.Instance = I2C2;
    hi2c2.Init.OwnAddress1 = (uint16_t)(addr << 1);
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c2) != HAL_OK)
    {
        LOG_ERROR("slave init failed");
        return false;
    }

    (void)HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE);
    (void)HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0);

    HAL_NVIC_SetPriority(I2C2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(I2C2_IRQn);

    m_active = true;

    if (HAL_I2C_EnableListen_IT(&hi2c2) != HAL_OK)
    {
        LOG_ERROR("listen start failed");
        m_active = false;
        return false;
    }

    LOG_INFO("slave listen at 0x%02X", addr);
    return true;
}

void i2c_slave_hw_stop(void)
{
    if (!m_active)
        return;

    // NOTE: остановка железа завершает транзакцию, поэтому индикатор
    // активности нужно погасить, даже если колбэков HAL уже не будет
    activity_end();

    m_active = false;
    HAL_NVIC_DisableIRQ(I2C2_IRQn);
    (void)HAL_I2C_DeInit(&hi2c2);
    reset_xfer();

    // Возвращаем конфигурацию master, чтобы i2c_init() видел готовую шину
    MX_I2C2_Init();
}

bool i2c_slave_hw_active(void)
{
    return m_active;
}

bool i2c_slave_hw_supply(const uint8_t *data, uint16_t len)
{
    uint8_t fallback = 0xFF;

    if (!m_active || !m_read_pending)
        return false;

    if ((data == NULL) || (len == 0))
    {
        data = &fallback;
        len = 1;
    }

    if (len > sizeof(m_tx_buf))
        len = sizeof(m_tx_buf);

    memcpy(m_tx_buf, data, len);

    if (__HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_ADDR) == RESET)
    {
        // Мастер уже завершил транзакцию, передавать некому
        m_read_pending = false;
        return false;
    }

    m_tx_len = len;
    m_read_pending = false;

    if (HAL_I2C_Slave_Seq_Transmit_IT(&hi2c2, m_tx_buf, len,
                                      I2C_FIRST_AND_LAST_FRAME) != HAL_OK)
    {
        LOG_WARNING("slave tx start failed");
        return false;
    }

    return true;
}
//<<----------------------

extern "C"
{
    /**
     * @brief Прерывание I2C2 (обслуживание slave-режима)
     */
    void I2C2_IRQHandler(void)
    {
        HAL_I2C_EV_IRQHandler(&hi2c2);
        HAL_I2C_ER_IRQHandler(&hi2c2);
    }

    void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t direction,
                              uint16_t match_code)
    {
        if (!m_active || (hi2c->Instance != I2C2))
            return;

        activity_begin();

        m_addr = (uint8_t)((match_code >> 1) & 0x7F);

        if (direction == I2C_DIRECTION_TRANSMIT)
        {
            reset_xfer();
            m_xfer_active = true;

            if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, m_rx_buf,
                                             sizeof(m_rx_buf),
                                             I2C_FIRST_AND_LAST_FRAME) !=
                HAL_OK)
            {
                LOG_WARNING("slave rx start failed");
                activity_end();
            }
            return;
        }

        // Фаза чтения: если перед ней была запись, отдаём её хосту
        if (hi2c->State == HAL_I2C_STATE_BUSY_RX_LISTEN)
        {
            uint16_t written = 0;
            if (hi2c->XferSize > hi2c->XferCount)
                written = (uint16_t)(hi2c->XferSize - hi2c->XferCount);
            if (written > sizeof(m_rx_buf))
                written = sizeof(m_rx_buf);

            m_xfer_active = true;
            m_write_len = written;
            m_write_reported = true;
            i2c_slave_service_on_write(m_addr, m_rx_buf, written, true);
        }
        else
        {
            reset_xfer();
            m_xfer_active = true;
            i2c_slave_service_on_read(m_addr);
        }

        // ADDR не сбрасываем: SCL удерживается до i2c_slave_hw_supply()
        m_read_pending = true;
        }

    void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
    {
        if (!m_active || (hi2c->Instance != I2C2))
            return;

        uint16_t received = 0;
        if (hi2c->XferSize > hi2c->XferCount)
            received = (uint16_t)(hi2c->XferSize - hi2c->XferCount);
        if (received > sizeof(m_rx_buf))
            received = sizeof(m_rx_buf);

        m_write_len = received;
        finish_xfer(PROTO_STATUS_OK);
    }

    void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
    {
        if (!m_active || (hi2c->Instance != I2C2))
            return;

        m_read_len = m_tx_len;
        finish_xfer(PROTO_STATUS_OK);
    }

    void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
    {
        if (!m_active || (hi2c->Instance != I2C2))
            return;

        finish_xfer(PROTO_STATUS_OK);
    }

    void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
    {
        if (!m_active || (hi2c->Instance != I2C2))
            return;

        finish_xfer(PROTO_STATUS_BUS_ERROR);
    }
}