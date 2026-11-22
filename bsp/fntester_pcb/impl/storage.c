/**
 * @file storage.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Заглушка файловой системы для fntester_pcb (STM32F072)
 * @version 0.1
 * @date 2026-11-22
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 * NOTE: на плате нет SD-карты/FatFS. Реализация оставлена только для того,
 *       чтобы шаблонное приложение собиралось и линковалось. При появлении
 *       энергонезависимого хранилища (Flash/EEPROM/внешняя память) заменить.
 */
#include <stddef.h>
#include <stdint.h>

#include "app/storage/storage.h"

//>>---------------------- Public
int storage_init(void)
{
    return 0;
}

int storage_file_create(const char *name, file_t *f)
{
    (void)name;
    (void)f;
    return -1;
}

int storage_file_open(const char *name, file_t *f)
{
    (void)name;
    (void)f;
    return -1;
}

int storage_file_write(file_t *f, const void *data, size_t size)
{
    (void)f;
    (void)data;
    (void)size;
    return -1;
}

int storage_file_read(file_t *f, void *data, size_t size)
{
    (void)f;
    (void)data;
    (void)size;
    return -1;
}

int storage_file_seek(file_t *f, uint32_t offset)
{
    (void)f;
    (void)offset;
    return -1;
}

uint32_t storage_file_size(file_t *f)
{
    (void)f;
    return 0;
}

int storage_file_close(file_t *f)
{
    (void)f;
    return -1;
}

int storage_file_delete(const char *name)
{
    (void)name;
    return -1;
}

int storage_file_exists(const char *name)
{
    (void)name;
    return 0;
}
//<<----------------------