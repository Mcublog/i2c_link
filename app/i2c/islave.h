#ifndef APP_I2C_ISLAVE_H
#define APP_I2C_ISLAVE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Минимальный допустимый 7-битный адрес slave */
#define I2C_SLAVE_ADDR_MIN (0x08)
/** @brief Максимальный допустимый 7-битный адрес slave */
#define I2C_SLAVE_ADDR_MAX (0x77)

    /**
     * @brief Запустить шину в режиме slave (прослушивание своего адреса)
     *
     * Платформенная реализация переводит контроллер I2C в slave-режим,
     * включает прерывания и clock stretching. Входящие транзакции
     * сообщаются в сервис через i2c_slave_service_on_*().
     *
     * @param addr 7-битный адрес slave (I2C_SLAVE_ADDR_MIN..MAX)
     * @return true - режим включён
     */
    bool i2c_slave_hw_start(uint8_t addr);

    /**
     * @brief Остановить slave-режим
     *
     * Контроллер освобождается; для master-режима его нужно
     * инициализировать заново.
     */
    void i2c_slave_hw_stop(void);

    /**
     * @brief Проверить, активен ли slave-режим
     *
     * @return true - устройство слушает шину как slave
     */
    bool i2c_slave_hw_active(void);

    /**
     * @brief Отдать данные для текущей фазы чтения
     *
     * Вызывается сервисом после ответа хоста. Пока данные не переданы,
     * платформа удерживает SCL (clock stretching), поэтому мастер ждёт.
     *
     * @param data байты для передачи мастеру
     * @param len длина данных (0 - передать один байт 0xFF)
     * @return true - передача запущена
     */
    bool i2c_slave_hw_supply(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // APP_I2C_ISLAVE_H
