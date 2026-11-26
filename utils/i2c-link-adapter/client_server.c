/**
 * @file client_server.c
 * @brief UNIX-сокет-сервер: один клиент за раз
 * @version 0.1
 * @date 2026-11-26
 *
 */
#include "client_server.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

//>>---------------------- Log control
#define LOG_MODULE_NAME sock
#define LOG_MODULE_LEVEL (4)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Сообщение клиенту, когда порт уже занят */
static const char kBusyMessage[] = "ERR BUSY another client is connected\n";
//<<----------------------

//>>---------------------- Private
/**
 * @brief Перевести дескриптор в неблокирующий режим
 *
 * @param fd файловый дескриптор
 * @return true - успешно
 */
static bool make_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return false;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
//<<----------------------

//>>---------------------- Public
bool client_server_init(client_server_t *srv, const char *path)
{
    if ((srv == NULL) || (path == NULL) || (path[0] == '\0'))
        return false;

    size_t len = strlen(path);
    if (len >= sizeof(srv->path))
        return false;

    srv->listen_fd = -1;
    srv->client_fd = -1;
    memcpy(srv->path, path, len + 1);
    return true;
}

bool client_server_start(client_server_t *srv)
{
    if ((srv == NULL) || (srv->path[0] == '\0'))
        return false;
    if (srv->listen_fd >= 0)
        return true;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, srv->path, strlen(srv->path));

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        LOG_ERROR("socket failed: %s", strerror(errno));
        return false;
    }

    if (!make_nonblocking(fd))
    {
        LOG_ERROR("fcntl failed: %s", strerror(errno));
        (void)close(fd);
        return false;
    }

    (void)unlink(srv->path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        LOG_ERROR("bind %s failed: %s", srv->path, strerror(errno));
        (void)close(fd);
        return false;
    }

    if (listen(fd, 1) != 0)
    {
        LOG_ERROR("listen failed: %s", strerror(errno));
        (void)close(fd);
        (void)unlink(srv->path);
        return false;
    }

    srv->listen_fd = fd;
    LOG_INFO("waiting for clients on %s", srv->path);
    return true;
}

void client_server_stop(client_server_t *srv)
{
    if (srv == NULL)
        return;

    client_server_drop(srv);

    if (srv->listen_fd >= 0)
    {
        (void)close(srv->listen_fd);
        srv->listen_fd = -1;
    }

    if (srv->path[0] != '\0')
        (void)unlink(srv->path);
}

bool client_server_connected(const client_server_t *srv)
{
    return (srv != NULL) && (srv->client_fd >= 0);
}

client_accept_t client_server_accept(client_server_t *srv)
{
    if ((srv == NULL) || (srv->listen_fd < 0))
        return CLIENT_ACCEPT_ERROR;

    int fd = accept(srv->listen_fd, NULL, NULL);
    if (fd < 0)
    {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
            return CLIENT_ACCEPT_NONE;

        LOG_WARNING("accept failed: %s", strerror(errno));
        return CLIENT_ACCEPT_ERROR;
    }

    if (!make_nonblocking(fd))
    {
        LOG_ERROR("fcntl failed: %s", strerror(errno));
        (void)close(fd);
        return CLIENT_ACCEPT_ERROR;
    }

    if (srv->client_fd >= 0)
    {
        LOG_WARNING("second client rejected");
        (void)send(fd, kBusyMessage, sizeof(kBusyMessage) - 1, 0);
        (void)close(fd);
        return CLIENT_ACCEPT_REJECTED;
    }

    srv->client_fd = fd;
    LOG_INFO("client connected");
    return CLIENT_ACCEPT_NEW;
}

void client_server_drop(client_server_t *srv)
{
    if ((srv == NULL) || (srv->client_fd < 0))
        return;

    (void)close(srv->client_fd);
    srv->client_fd = -1;
    LOG_INFO("client disconnected");
}

int32_t client_server_read(client_server_t *srv, uint8_t *buf, uint32_t cap)
{
    if ((srv == NULL) || (srv->client_fd < 0) || (buf == NULL) || (cap == 0))
        return 0;

    ssize_t size = recv(srv->client_fd, buf, cap, 0);
    if (size > 0)
        return (int32_t)size;
    if (size == 0)
        return -1;

    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
        return 0;

    return -1;
}

int32_t client_server_write(client_server_t *srv, const uint8_t *data,
                            uint32_t size)
{
    if ((srv == NULL) || (srv->client_fd < 0) || (data == NULL) || (size == 0))
        return 0;

    ssize_t written = send(srv->client_fd, data, size, MSG_NOSIGNAL);
    if (written > 0)
        return (int32_t)written;

    if ((written < 0) &&
        ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)))
        return 0;

    return -1;
}
//<<----------------------