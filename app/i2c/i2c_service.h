#ifndef APP_I2C_I2C_SERVICE_H
#define APP_I2C_I2C_SERVICE_H

#include "app/proto/proto.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализировать сервис команд I2C
     *
     * Выполняет инициализацию шины.
     */
    void i2c_service_init(void);

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