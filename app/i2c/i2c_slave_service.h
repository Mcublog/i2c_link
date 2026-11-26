#ifndef APP_I2C_I2C_SLAVE_SERVICE_H
#define APP_I2C_I2C_SLAVE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/proto/link.h"
#include "app/proto/proto.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализировать сервис slave-режима
     *
     * @param link канальный уровень протокола для отправки событий
     */
    void i2c_slave_service_init(proto_link_t *link);

    /**
     * @brief Сбросить состояние сервиса (очередь событий и ожидание)
     */
    void i2c_slave_service_reset(void);

    /**
     * @brief Отправить накопленные события в протокол
     *
     * Вызывается из основного цикла. Неотправленные события остаются
     * в очереди до следующего вызова.
     */
    void i2c_slave_service_poll(void);

    /**
     * @brief Проверить, есть ли незавершённая транзакция
     *
     * @return true - сервис ждёт ответ хоста или отправляет события
     */
    bool i2c_slave_service_busy(void);

    /**
     * @brief Обработать фазу записи от внешнего мастера
     *
     * Вызывается платформой из прерывания при завершении фазы записи.
     *
     * @param addr адрес slave, к которому обратился мастер
     * @param data принятые байты
     * @param len число принятых байт
     * @param read_follows true - далее следует фаза чтения (repeated start)
     */
    void i2c_slave_service_on_write(uint8_t addr, const uint8_t *data,
                                    uint16_t len, bool read_follows);

    /**
     * @brief Обработать запрос фазы чтения от внешнего мастера
     *
     * Вызывается платформой из прерывания при адресе с направлением
     * чтения, если записи перед этим не было.
     *
     * @param addr адрес slave, к которому обратился мастер
     */
    void i2c_slave_service_on_read(uint8_t addr);

    /**
     * @brief Обработать завершение транзакции
     *
     * Вызывается платформой из прерывания после STOP/повторного старта.
     *
     * @param addr адрес slave
     * @param written число записанных мастером байт
     * @param read число прочитанных мастером байт
     * @param status результат транзакции
     */
    void i2c_slave_service_on_done(uint8_t addr, uint16_t written,
                                   uint16_t read, proto_status_t status);

    /**
     * @brief Обработать ответ хоста на событие SLAVE_EVENT
     *
     * @param req разобранный кадр-ответ (команда PROTO_CMD_SLAVE_EVENT)
     */
    void i2c_slave_service_on_reply(const proto_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // APP_I2C_I2C_SLAVE_SERVICE_H
