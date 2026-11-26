#ifndef I2C_LINK_ADAPTER_CLI_H
#define I2C_LINK_ADAPTER_CLI_H

#include <stdbool.h>
#include <stdint.h>

#include "app/proto/proto.h"
#include "cdc_port.h"
#include "client_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Адрес slave по умолчанию */
    #define CLI_DEFAULT_ADDR (0x42)
    /** @brief Срок ответа клиента по умолчанию, мс */
    #define CLI_DEFAULT_REPLY_TIMEOUT (20)

    /**
     * @brief Параметры запуска утилиты
     */
    typedef struct
    {
        char tty[CDC_PORT_PATH_MAX]; /**< путь к CDC-порту */
        char listen[CLIENT_SERVER_PATH_MAX]; /**< путь к UNIX-сокету */
        bool semantic; /**< семантический режим вместо прозрачного */
        bool configure; /**< настраивать режим устройства при старте */
        uint8_t mode; /**< нужный режим (PROTO_MODE_*) */
        uint8_t addr; /**< нужный адрес slave */
        uint16_t reply_timeout_ms; /**< срок ответа клиента, мс */
    } cli_opts_t;

    /**
     * @brief Разобрать аргументы командной строки
     *
     * При --help печатает подсказку и возвращает false с need_exit=true.
     *
     * @param argc число аргументов
     * @param argv аргументы
     * @param opts результат разбора
     * @param need_exit true - нужно завершиться с кодом 0
     * @return true - параметры корректны
     */
    bool cli_parse(int argc, char **argv, cli_opts_t *opts, bool *need_exit);

    /**
     * @brief Напечатать подсказку по запуску
     */
    void cli_usage(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_LINK_ADAPTER_CLI_H