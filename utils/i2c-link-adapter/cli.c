/**
 * @file cli.c
 * @brief Разбор аргументов командной строки адаптера
 * @version 0.1
 * @date 2026-11-26
 *
 */
#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//>>---------------------- Log control
#define LOG_MODULE_NAME cli
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Private
/**
 * @brief Разобрать строку в беззнаковое число (десятичное или 0x..)
 *
 * @param text строка
 * @param out результат
 * @return true - успешно
 */
static bool parse_uint(const char *text, unsigned long *out)
{
    char *end = NULL;

    if ((text == NULL) || (text[0] == '\0'))
        return false;

    unsigned long value = strtoul(text, &end, 0);
    if ((end == NULL) || (*end != '\0'))
        return false;

    *out = value;
    return true;
}

/**
 * @brief Проверить, что значения адреса и режима допустимы
 *
 * @param opts параметры
 * @return true - корректно
 */
static bool validate(const cli_opts_t *opts)
{
    if ((opts->addr < 0x08) || (opts->addr > 0x77))
    {
        LOG_ERROR("slave address must be 0x08..0x77");
        return false;
    }

    if ((opts->mode != PROTO_MODE_MASTER) && (opts->mode != PROTO_MODE_SLAVE))
    {
        LOG_ERROR("mode must be master or slave");
        return false;
    }

    return true;
}
//<<----------------------

//>>---------------------- Public
void cli_usage(void)
{
    printf("i2c-link-adapter — мост между UNIX-сокетом и CDC-портом платы\n"
           "\n"
           "Usage: i2c-link-adapter [options]\n"
           "\n"
           "Options:\n"
           "  --tty <path>          CDC-порт платы (default: /dev/ttyACM0)\n"
           "  --listen <path>       UNIX-сокет (default: /tmp/i2c_link.sock)\n"
           "  --semantic            семантический режим (события текстом)\n"
           "  --addr <0x08..0x77>   адрес slave (default: 0x42)\n"
           "  --mode <slave|master> режим шины (default: slave)\n"
           "  --reply-timeout <ms>  срок ответа, default 20 (0 - без)\n"
           "  --no-config           не настраивать режим при старте\n"
           "  -h, --help            эта подсказка\n"
           "\n"
           "Examples:\n"
           "  i2c-link-adapter --semantic --addr 0x42\n"
           "  i2c-link-adapter --raw --mode master\n");
}

bool cli_parse(int argc, char **argv, cli_opts_t *opts, bool *need_exit)
{
    if ((opts == NULL) || (need_exit == NULL))
        return false;

    memset(opts, 0, sizeof(*opts));
    memcpy(opts->tty, "/dev/ttyACM0", sizeof("/dev/ttyACM0"));
    memcpy(opts->listen, "/tmp/i2c_link.sock", sizeof("/tmp/i2c_link.sock"));
    opts->semantic = false;
    opts->configure = true;
    opts->mode = PROTO_MODE_SLAVE;
    opts->addr = CLI_DEFAULT_ADDR;
    opts->reply_timeout_ms = CLI_DEFAULT_REPLY_TIMEOUT;
    *need_exit = false;

    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];

        if ((strcmp(arg, "-h") == 0) || (strcmp(arg, "--help") == 0))
        {
            cli_usage();
            *need_exit = true;
            return false;
        }

        if (strcmp(arg, "--semantic") == 0)
        {
            opts->semantic = true;
            continue;
        }

        if (strcmp(arg, "--raw") == 0)
        {
            opts->semantic = false;
            continue;
        }

        if (strcmp(arg, "--no-config") == 0)
        {
            opts->configure = false;
            continue;
        }

        if ((i + 1) >= argc)
        {
            LOG_ERROR("missing value for %s", arg);
            return false;
        }

        const char *value = argv[++i];

        if (strcmp(arg, "--tty") == 0)
        {
            if (strlen(value) >= sizeof(opts->tty))
            {
                LOG_ERROR("tty path too long");
                return false;
            }
            strcpy(opts->tty, value);
            continue;
        }

        if (strcmp(arg, "--listen") == 0)
        {
            if (strlen(value) >= sizeof(opts->listen))
            {
                LOG_ERROR("socket path too long");
                return false;
            }
            strcpy(opts->listen, value);
            continue;
        }

        if (strcmp(arg, "--addr") == 0)
        {
            unsigned long number = 0;
            if (!parse_uint(value, &number) || (number > 0xFF))
            {
                LOG_ERROR("bad address: %s", value);
                return false;
            }
            opts->addr = (uint8_t)number;
            continue;
        }

        if (strcmp(arg, "--mode") == 0)
        {
            if (strcmp(value, "slave") == 0)
                opts->mode = PROTO_MODE_SLAVE;
            else if (strcmp(value, "master") == 0)
                opts->mode = PROTO_MODE_MASTER;
            else
            {
                LOG_ERROR("bad mode: %s", value);
                return false;
            }
            continue;
        }

        if (strcmp(arg, "--reply-timeout") == 0)
        {
            unsigned long number = 0;
            if (!parse_uint(value, &number) || (number > 60000))
            {
                LOG_ERROR("bad reply timeout: %s", value);
                return false;
            }
            opts->reply_timeout_ms = (uint16_t)number;
            continue;
        }

        LOG_ERROR("unknown option: %s", arg);
        return false;
    }

    return validate(opts);
}
//<<----------------------