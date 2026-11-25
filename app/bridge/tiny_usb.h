#ifndef APP_BRIDGE_TINY_USB_H
#define APP_BRIDGE_TINY_USB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Максимальный размер данных одной транзакции, байт */
#define TINY_USB_BUF_SIZE (256)

/** @brief Команда: эхо-проверка канала */
#define TINY_USB_CMD_ECHO (0)
/** @brief Команда: маска поддержанных функций I2C */
#define TINY_USB_CMD_GET_FUNC (1)
/** @brief Команда: задержка перед чтением, мкс */
#define TINY_USB_CMD_SET_DELAY (2)
/** @brief Команда: статус последней транзакции */
#define TINY_USB_CMD_GET_STATUS (3)
/** @brief Команда: транзакция на шине I2C */
#define TINY_USB_CMD_IO (4)

/** @brief Флаг команды I2C_IO: первый сегмент транзакции */
#define TINY_USB_IO_BEGIN (0x01)
/** @brief Флаг команды I2C_IO: последний сегмент транзакции */
#define TINY_USB_IO_END (0x02)
/** @brief Маска флагов в поле bRequest */
#define TINY_USB_IO_FLAGS (TINY_USB_IO_BEGIN | TINY_USB_IO_END)

/** @brief Бит направления IN в bmRequestType */
#define TINY_USB_DIR_IN (0x80)

/** @brief Статус: транзакций ещё не было */
#define TINY_USB_STATUS_IDLE (0)
/** @brief Статус: адрес устройства подтверждён */
#define TINY_USB_STATUS_ACK (1)
/** @brief Статус: адрес устройства не подтверждён (NACK) */
#define TINY_USB_STATUS_NAK (2)

/** @brief Маска функций I2C для GET_FUNC
 *
 * Соответствует I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL из linux/i2c.h:
 * базовые транзакции и SMBus-эмуляция на уровне ядра.
 */
#define TINY_USB_FUNC_MASK (0x0EFF0009u)

    /**
     * @brief Результат разбора setup-запроса
     */
    typedef enum
    {
        TINY_USB_RESULT_STALL = 0, /**< запрос не поддерживается */
        TINY_USB_RESULT_IMMEDIATE, /**< ответ готов в буфере */
        TINY_USB_RESULT_ARM_OUT /**< нужно принять данные записи */
    } tiny_usb_result_t;

    /**
     * @brief Разобранный setup-запрос control-трансфера
     */
    typedef struct
    {
        uint8_t type; /**< bmRequestType */
        uint8_t request; /**< bRequest (с флагами для I2C_IO) */
        uint16_t value; /**< wValue */
        uint16_t index; /**< wIndex (для I2C_IO - адрес устройства) */
        uint16_t length; /**< wLength */
    } tiny_usb_req_t;

    /**
     * @brief Состояние моста i2c-tiny-usb
     */
    typedef struct
    {
        uint8_t status; /**< статус последней транзакции */
        uint8_t addr; /**< адрес отложенной записи */
        bool write_pending; /**< ожидается приём данных записи */
        uint16_t delay_us; /**< задержка перед чтением, мкс */
    } tiny_usb_t;

    /**
     * @brief Инициализировать состояние моста
     *
     * @param ctx состояние
     */
    void tiny_usb_init(tiny_usb_t *ctx);

    /**
     * @brief Обработать setup-запрос
     *
     * Для TINY_USB_RESULT_IMMEDIATE ответ (и его длина) находится в
     * @p data и @p len; при @p len == 0 отправляется только status stage.
     * Для TINY_USB_RESULT_ARM_OUT данные записи принимаются в @p data
     * и обрабатываются в tiny_usb_write().
     *
     * Чтение из I2C выполняется синхронно: при NACK данные заполняются
     * значением 0xFF, а статус выставляется в TINY_USB_STATUS_NAK.
     *
     * @param ctx состояние
     * @param req разобранный запрос
     * @param data буфер ответа/приёма данных, TINY_USB_BUF_SIZE байт
     * @param len длина ответа
     * @return tiny_usb_result_t результат разбора
     */
    tiny_usb_result_t tiny_usb_setup(tiny_usb_t *ctx,
                                     const tiny_usb_req_t *req,
                                     uint8_t *data, uint16_t *len);

    /**
     * @brief Завершить отложенную запись в I2C
     *
     * Выполняет транзакцию и обновляет статус последней операции.
     *
     * @param ctx состояние
     * @param data принятые данные
     * @param len длина данных
     */
    void tiny_usb_write(tiny_usb_t *ctx, const uint8_t *data,
                        uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // APP_BRIDGE_TINY_USB_H
