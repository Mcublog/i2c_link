#ifndef I2C_LINK_ADAPTER_CLIENT_SERVER_H
#define I2C_LINK_ADAPTER_CLIENT_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Максимальная длина пути к сокету */
    #define CLIENT_SERVER_PATH_MAX (108)

    /**
     * @brief UNIX-сокет-сервер для одного клиента
     */
    typedef struct
    {
        int listen_fd; /**< слушающий сокет (-1 - не запущен) */
        int client_fd; /**< подключённый клиент (-1 - нет клиента) */
        char path[CLIENT_SERVER_PATH_MAX]; /**< путь к сокету */
    } client_server_t;

    /**
     * @brief Инициализировать структуру сервера (без запуска)
     *
     * @param srv сервер
     * @param path путь к UNIX-сокету
     * @return true - путь корректен
     */
    bool client_server_init(client_server_t *srv, const char *path);

    /**
     * @brief Создать UNIX-сокет и начать слушать
     *
     * Старый файл сокета удаляется, если он существует.
     *
     * @param srv сервер
     * @return true - сервер запущен
     */
    bool client_server_start(client_server_t *srv);

    /**
     * @brief Остановить сервер и удалить файл сокета
     *
     * @param srv сервер
     */
    void client_server_stop(client_server_t *srv);

    /**
     * @brief Проверить, есть ли подключённый клиент
     *
     * @param srv сервер
     * @return true - клиент подключён
     */
    bool client_server_connected(const client_server_t *srv);

    /**
     * @brief Результат приёма подключения
     */
    typedef enum
    {
        CLIENT_ACCEPT_NONE = 0, /**< новых подключений нет */
        CLIENT_ACCEPT_NEW = 1, /**< клиент подключён */
        CLIENT_ACCEPT_REJECTED = 2, /**< лишний клиент отвергнут */
        CLIENT_ACCEPT_ERROR = -1 /**< ошибка сервера */
    } client_accept_t;

    /**
     * @brief Принять подключение
     *
     * Если клиент уже подключён, новое подключение отвергается.
     *
     * @param srv сервер
     * @return client_accept_t результат
     */
    client_accept_t client_server_accept(client_server_t *srv);

    /**
     * @brief Отключить текущего клиента
     *
     * @param srv сервер
     */
    void client_server_drop(client_server_t *srv);

    /**
     * @brief Прочитать порцию данных от клиента
     *
     * @param srv сервер
     * @param buf буфер назначения
     * @param cap ёмкость буфера
     * @return int32_t число байтов (0 - данных нет),
     *         -1 - клиент отключился
     */
    int32_t client_server_read(client_server_t *srv, uint8_t *buf,
                               uint32_t cap);

    /**
     * @brief Записать порцию данных клиенту
     *
     * @param srv сервер
     * @param data данные
     * @param size размер порции
     * @return int32_t число записанных байтов (0 - буфер занят),
     *         -1 - клиент отключился
     */
    int32_t client_server_write(client_server_t *srv, const uint8_t *data,
                                uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // I2C_LINK_ADAPTER_CLIENT_SERVER_H