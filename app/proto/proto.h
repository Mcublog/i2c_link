#ifndef APP_PROTO_PROTO_H
#define APP_PROTO_PROTO_H

#include <stdint.h>

//>>---------------------- Local
/** @brief Версия протокола */
#define PROTO_VERSION (1)
/** @brief Сдвиг поля версии в байте ver_cmd */
#define PROTO_VERSION_SHIFT (4)
/** @brief Маска поля версии в байте ver_cmd */
#define PROTO_VERSION_MASK (0xF0)
/** @brief Маска поля команды в байте ver_cmd */
#define PROTO_CMD_MASK (0x0F)

/** @brief Максимальная длина payload (data) в кадре */
#define PROTO_MAX_DATA (128)

/** @brief Бит направления в поле msg_id: 1 - ответ/событие */
#define PROTO_MSG_DIR_RESPONSE (0x80)
/** @brief Маска номера сообщения в поле msg_id */
#define PROTO_MSG_ID_MASK (0x7F)

/** @brief Маска размера регистра в поле flags */
#define PROTO_REG_SIZE_MASK (0x03)
/** @brief Маска зарезервированных битов поля flags */
#define PROTO_FLAGS_RESERVED_MASK (0xFC)

/** @brief Размер заголовка запроса (байт) */
#define PROTO_REQ_HDR_SIZE (7)
/** @brief Размер заголовка ответа (байт) */
#define PROTO_RESP_HDR_SIZE (6)
/** @brief Размер CRC-16 (байт) */
#define PROTO_CRC_SIZE (2)

/** @brief Максимальный размер сырого кадра (байт) */
#define PROTO_MAX_RAW_FRAME \
    (PROTO_REQ_HDR_SIZE + PROTO_MAX_DATA + PROTO_CRC_SIZE)
/** @brief Максимальный размер COBS/R-кадра без терминатора (байт) */
#define PROTO_MAX_COBS_FRAME \
    (PROTO_MAX_RAW_FRAME + ((PROTO_MAX_RAW_FRAME + 253u) / 254u))
/** @brief Максимальный размер кадра на проводе с терминатором (байт) */
#define PROTO_MAX_ENCODED_FRAME (PROTO_MAX_COBS_FRAME + 1)

/** @brief Размер битовой карты результата SCAN (байт, 128 адресов) */
#define PROTO_SCAN_BITMAP_SIZE (16)
/** @brief Размер payload ответа INFO (байт) */
#define PROTO_INFO_SIZE (8)
/** @brief Размер payload ответа STATS (байт) */
#define PROTO_STATS_SIZE (14)
/** @brief Размер кольцевого буфера передачи (степень двойки) */
#define PROTO_TX_RING_SIZE (512)
/** @brief Лимит неудачных попыток отправки подряд */
#define PROTO_TX_RETRY_LIMIT (500)

/**
 * @brief Команды протокола
 */
typedef enum
{
    PROTO_CMD_READ = 0x01, /**< чтение из I2C */
    PROTO_CMD_WRITE = 0x02, /**< запись в I2C */
    PROTO_CMD_SCAN = 0x03, /**< сканирование шины */
    PROTO_CMD_INFO = 0x04, /**< информация о мосте */
    PROTO_CMD_PING = 0x05, /**< эхо-проверка канала */
    PROTO_CMD_BUS_RECOVER = 0x06, /**< освобождение шины */
    PROTO_CMD_STATS = 0x07, /**< счётчики ошибок */
    PROTO_CMD_EVENT = 0x09 /**< первое значение диапазона событий */
} proto_cmd_t;

/**
 * @brief Коды результата протокола
 */
typedef enum
{
    PROTO_STATUS_OK = 0x00, /**< успешно */
    PROTO_STATUS_NACK_ADDR = 0x01, /**< нет ACK на адрес */
    PROTO_STATUS_NACK_DATA = 0x02, /**< нет ACK на данные */
    PROTO_STATUS_TIMEOUT = 0x03, /**< таймаут транзакции */
    PROTO_STATUS_BUS_ERROR = 0x04, /**< ошибка шины */
    PROTO_STATUS_BUSY = 0x05, /**< шина занята */
    PROTO_STATUS_PARAM = 0x06, /**< неверные параметры */
    PROTO_STATUS_UNKNOWN_CMD = 0x07, /**< неизвестная команда */
    PROTO_STATUS_CRC = 0x08, /**< ошибка CRC */
    PROTO_STATUS_FRAME = 0x09, /**< битая структура кадра */
    PROTO_STATUS_OVERFLOW = 0x0A, /**< переполнение буфера */
    PROTO_STATUS_NOT_SUPPORTED = 0x0B /**< не поддерживается */
} proto_status_t;

/**
 * @brief Разобранный кадр запроса
 */
typedef struct
{
    proto_cmd_t cmd; /**< команда */
    uint8_t msg_id; /**< номер сообщения (без бита направления) */
    uint8_t flags; /**< флаги запроса */
    uint8_t addr; /**< 7-битный адрес I2C */
    uint8_t len; /**< длина payload (для READ - сколько читать) */
    uint16_t reg; /**< адрес регистра */
    const uint8_t *data; /**< payload запроса (может быть NULL) */
} proto_req_t;

/**
 * @brief Формируемый кадр ответа
 */
typedef struct
{
    proto_cmd_t cmd; /**< команда */
    uint8_t msg_id; /**< номер сообщения (без бита направления) */
    uint8_t flags; /**< флаги (эхо запроса) */
    uint8_t addr; /**< адрес (эхо запроса) */
    uint8_t len; /**< длина payload */
    proto_status_t status; /**< код результата */
    const uint8_t *data; /**< payload ответа (может быть NULL) */
} proto_resp_t;

/**
 * @brief Счётчики протокола
 */
typedef struct
{
    uint16_t rx_ok; /**< принято валидных запросов */
    uint16_t rx_crc_err; /**< ошибок CRC */
    uint16_t rx_overflow; /**< переполнений буфера приёма */
    uint16_t rx_frame_err; /**< ошибок структуры/кадра */
    uint16_t tx_drop; /**< отброшено кадров при передаче */
} proto_stats_t;

#endif // APP_PROTO_PROTO_H