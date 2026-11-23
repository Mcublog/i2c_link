#ifndef APP_I2C_II2C_H
#define APP_I2C_II2C_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Размер битовой карты сканирования (128 адресов) */
#define I2C_SCAN_BITMAP_SIZE (16)

    /**
     * @brief Результат операции на шине I2C
     */
    typedef enum
    {
        I2C_STATUS_OK = 0, /**< успешно */
        I2C_STATUS_NACK = 1, /**< устройство не ответило */
        I2C_STATUS_TIMEOUT = 2, /**< таймаут транзакции */
        I2C_STATUS_BUS = 3, /**< ошибка/занятость шины */
        I2C_STATUS_PARAM = 4, /**< неверные параметры */
        I2C_STATUS_BUSY = 5, /**< шина занята */
        I2C_STATUS_NOT_SUPPORTED = 6 /**< не поддерживается */
    } i2c_status_t;

    /**
     * @brief Инициализировать шину I2C
     *
     * @return true - успешно
     */
    bool i2c_init(void);

    /**
     * @brief Проверить наличие устройства на шине
     *
     * @param addr 7-битный адрес устройства
     * @return i2c_status_t
     */
    i2c_status_t i2c_probe(uint8_t addr);

    /**
     * @brief Прочитать данные из устройства
     *
     * При @p reg_size > 0 сначала выполняется запись регистра, затем
     * repeated start и чтение. При @p len == 0 выполняется только проба
     * адреса.
     *
     * @param addr 7-битный адрес устройства
     * @param reg адрес регистра
     * @param reg_size размер регистра в байтах (0, 1 или 2)
     * @param data буфер для данных
     * @param len количество байт для чтения
     * @return i2c_status_t
     */
    i2c_status_t i2c_read(uint8_t addr, uint16_t reg, uint8_t reg_size,
                          uint8_t *data, uint16_t len);

    /**
     * @brief Записать данные в устройство
     *
     * При @p reg_size > 0 адрес регистра передаётся перед данными.
     *
     * @param addr 7-битный адрес устройства
     * @param reg адрес регистра
     * @param reg_size размер регистра в байтах (0, 1 или 2)
     * @param data данные
     * @param len количество байт для записи
     * @return i2c_status_t
     */
    i2c_status_t i2c_write(uint8_t addr, uint16_t reg, uint8_t reg_size,
                           const uint8_t *data, uint16_t len);

    /**
     * @brief Просканировать шину
     *
     * @param from первый адрес диапазона
     * @param to последний адрес диапазона (включительно)
     * @param bitmap битовая карта, 16 байт
     * @return i2c_status_t
     */
    i2c_status_t i2c_scan(uint8_t from, uint8_t to,
                          uint8_t bitmap[I2C_SCAN_BITMAP_SIZE]);

    /**
     * @brief Освободить зависшую шину (9 импульсов SCL + STOP)
     *
     * @return i2c_status_t
     */
    i2c_status_t i2c_recover(void);

    /**
     * @brief Получить текущую скорость шины
     *
     * @return uint32_t скорость в кГц
     */
    uint32_t i2c_speed_khz(void);

#ifdef __cplusplus
}
#endif

#endif // APP_I2C_II2C_H