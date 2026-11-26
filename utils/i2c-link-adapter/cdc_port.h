#ifndef I2C_LINK_ADAPTER_CDC_PORT_H
#define I2C_LINK_ADAPTER_CDC_PORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Максимальная длина пути к устройству */
    #define CDC_PORT_PATH_MAX (256)

    /**
     * @brief Подключение к CDC-порту платы (виртуальный COM-порт)
     */
    typedef struct
    {
        int fd; /**< файловый дескриптор (-1 - не открыт) */
        char path[CDC_PORT_PATH_MAX]; /**< путь к устройству */
    } cdc_port_t;

    /**
     * @brief Инициализировать структуру порта (без открытия)
     *
     * @param port порт
     * @param path путь к устройству (например, /dev/ttyACM0)
     * @return true - путь корректен
     */
    bool cdc_port_init(cdc_port_t *port, const char *path);

    /**
     * @brief Открыть порт и настроить его в raw-режим
     *
     * Порт открывается неблокирующим. Если устройство уже открыто,
     * функция ничего не делает.
     *
     * @param port порт
     * @return true - порт открыт
     */
    bool cdc_port_open(cdc_port_t *port);

    /**
     * @brief Закрыть порт
     *
     * @param port порт
     */
    void cdc_port_close(cdc_port_t *port);

    /**
     * @brief Проверить, что порт открыт
     *
     * @param port порт
     * @return true - порт открыт
     */
    bool cdc_port_is_open(const cdc_port_t *port);

    /**
     * @brief Прочитать порцию данных из порта
     *
     * @param port порт
     * @param buf буфер назначения
     * @param cap ёмкость буфера
     * @return int32_t число байтов (0 - данных нет),
     *         -1 - устройство потеряно
     */
    int32_t cdc_port_read(cdc_port_t *port, uint8_t *buf, uint32_t cap);

    /**
     * @brief Записать порцию данных в порт
     *
     * @param port порт
     * @param data данные
     * @param size размер порции
     * @return int32_t число записанных байтов (0 - передатчик занят),
     *         -1 - устройство потеряно
     */
    int32_t cdc_port_write(cdc_port_t *port, const uint8_t *data,
                           uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // I2C_LINK_ADAPTER_CDC_PORT_H