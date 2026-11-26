#ifndef APP_I2C_I2C_SERVICE_H
#define APP_I2C_I2C_SERVICE_H

#include "app/proto/link.h"
#include "app/proto/proto.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализировать сервис команд I2C
     *
     * Выполняет инициализацию шины в master-режиме и привязывает сервис
     * к канальному уровню протокола (для событий slave-режима).
     *
     * @param link канальный уровень протокола
     */
    void i2c_service_init(proto_link_t *link);

    /**
     * @brief Обслуживать события slave-режима
     *
     * Вызывается регулярно из основного цикла: отправляет накопленные
     * события в протокол.
     */
    void i2c_service_poll(void);

    /**
     * @brief Обработать запрос протокола
     *
     * @param req разобранный запрос
     * @param resp формируемый ответ
     * @param ctx контекст, используется proto_link_t
     */
    void i2c_service_handle(const proto_req_t *req, proto_resp_t *resp,
                            void *ctx);

#ifdef __cplusplus
}
#endif

#endif // APP_I2C_I2C_SERVICE_H
