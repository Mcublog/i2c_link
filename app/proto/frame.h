#ifndef APP_PROTO_FRAME_H
#define APP_PROTO_FRAME_H

#include <stdint.h>

#include "app/proto/proto.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Посчитать CRC-16/CCITT-FALSE
     *
     * @param data данные
     * @param size размер данных
     * @return uint16_t контрольная сумма
     */
    uint16_t proto_crc16(const uint8_t *data, uint32_t size);

    /**
     * @brief Разобрать сырой кадр запроса
     *
     * Кадр уже раскодирован из COBS/R и не содержит терминатора.
     *
     * @param raw сырой кадр
     * @param size размер кадра
     * @param req результат разбора
     * @return proto_status_t
     *         - PROTO_STATUS_OK — кадр корректен;
     *         - PROTO_STATUS_PARAM/UNKNOWN_CMD — заголовок заполнен, но
     *           запрос некорректен (нужно отправить ответ с ошибкой);
     *         - PROTO_STATUS_CRC/FRAME — кадр нужно молча отбросить.
     */
    proto_status_t proto_frame_parse(const uint8_t *raw, uint32_t size,
                                     proto_req_t *req);

    /**
     * @brief Собрать сырой кадр ответа
     *
     * @param buf буфер для кадра
     * @param cap размер буфера
     * @param resp формируемый ответ
     * @return uint32_t размер кадра или 0 при ошибке
     */
    uint32_t proto_frame_build_resp(uint8_t *buf, uint32_t cap,
                                    const proto_resp_t *resp);

#ifdef __cplusplus
}
#endif

#endif // APP_PROTO_FRAME_H