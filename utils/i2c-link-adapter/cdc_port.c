/**
 * @file cdc_port.c
 * @brief CDC-порт платы на хосте: termios, неблокирующие чтение и запись
 * @version 0.1
 * @date 2026-11-26
 *
 */
#include "cdc_port.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

//>>---------------------- Log control
#define LOG_MODULE_NAME cdc
#define LOG_MODULE_LEVEL (4)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Private
/**
 * @brief Настроить порт в raw-режим без эха и управляющих символов
 *
 * @param fd файловый дескриптор
 * @return true - успешно
 */
static bool configure_raw(int fd)
{
    struct termios tio;

    if (tcgetattr(fd, &tio) != 0)
        return false;

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= (tcflag_t)~CRTSCTS;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    // NOTE: для CDC скорость не важна, но termios её требует
    if ((cfsetispeed(&tio, B115200) != 0) ||
        (cfsetospeed(&tio, B115200) != 0))
        return false;

    if (tcsetattr(fd, TCSANOW, &tio) != 0)
        return false;

    (void)tcflush(fd, TCIOFLUSH);
    return true;
}
//<<----------------------

//>>---------------------- Public
bool cdc_port_init(cdc_port_t *port, const char *path)
{
    if ((port == NULL) || (path == NULL) || (path[0] == '\0'))
        return false;

    size_t len = strlen(path);
    if (len >= sizeof(port->path))
        return false;

    port->fd = -1;
    memcpy(port->path, path, len + 1);
    return true;
}

bool cdc_port_open(cdc_port_t *port)
{
    if (port == NULL)
        return false;
    if (port->fd >= 0)
        return true;

    int fd = open(port->path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        LOG_DEBUG("open %s failed: %s", port->path, strerror(errno));
        return false;
    }

    if (!configure_raw(fd))
    {
        LOG_WARNING("configure %s failed: %s", port->path, strerror(errno));
        (void)close(fd);
        return false;
    }

    port->fd = fd;
    LOG_INFO("opened %s", port->path);
    return true;
}

void cdc_port_close(cdc_port_t *port)
{
    if ((port == NULL) || (port->fd < 0))
        return;

    (void)close(port->fd);
    port->fd = -1;
}

bool cdc_port_is_open(const cdc_port_t *port)
{
    return (port != NULL) && (port->fd >= 0);
}

int32_t cdc_port_read(cdc_port_t *port, uint8_t *buf, uint32_t cap)
{
    if ((port == NULL) || (port->fd < 0) || (buf == NULL) || (cap == 0))
        return 0;

    ssize_t size = read(port->fd, buf, cap);
    if (size > 0)
        return (int32_t)size;

    // NOTE: на tty с VMIN=0/VTIME=0 read() возвращает 0, когда данных нет.
    // Реальную потерю устройства ловит poll() по POLLHUP/POLLERR.
    if (size == 0)
        return 0;

    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR))
        return 0;

    return -1;
}

int32_t cdc_port_write(cdc_port_t *port, const uint8_t *data, uint32_t size)
{
    if ((port == NULL) || (port->fd < 0) || (data == NULL) || (size == 0))
        return 0;

    ssize_t written = write(port->fd, data, size);
    if (written > 0)
        return (int32_t)written;

    if ((written < 0) &&
        ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)))
        return 0;

    return -1;
}
//<<----------------------