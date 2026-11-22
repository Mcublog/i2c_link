/**
 * @file storage.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Файловая система для ws407 (FATFS на SD-карте)
 * @version 0.1
 * @date 2026-08-02
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include <stdlib.h>
#include <string.h>

#include "app/storage/storage.h"

#include "ff.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME fsfat
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
//<<----------------------

//>>---------------------- Private
//<<----------------------

//>>---------------------- Public
int storage_init(void)
{
    return 0;
}

int storage_file_create(const char *name, file_t *f)
{
    FIL *fil = (FIL *)calloc(1, sizeof(FIL));
    if (!fil)
        return -1;

    FRESULT res = f_open(fil, name, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        LOG_ERROR("create %s error[%d]", name, res);
        free(fil);
        return -1;
    }

    f->handle = fil;
    f->size = (uint32_t)f_size(fil);
    return 0;
}

int storage_file_open(const char *name, file_t *f)
{
    FIL *fil = (FIL *)calloc(1, sizeof(FIL));
    if (!fil)
        return -1;

    FRESULT res = f_open(fil, name, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
    if (res != FR_OK)
    {
        LOG_ERROR("open %s error[%d]", name, res);
        free(fil);
        return -1;
    }

    f->handle = fil;
    f->size = (uint32_t)f_size(fil);
    return 0;
}

int storage_file_write(file_t *f, const void *data, size_t size)
{
    FIL *fil = (FIL *)f->handle;
    if (!fil)
        return -1;

    UINT written = 0;
    FRESULT res = f_write(fil, data, (UINT)size, &written);
    if (res != FR_OK || written != size)
    {
        LOG_ERROR("write error[%d] written[%d]", res, written);
        return -1;
    }

    f->size = (uint32_t)f_size(fil);
    return 0;
}

int storage_file_read(file_t *f, void *data, size_t size)
{
    FIL *fil = (FIL *)f->handle;
    if (!fil)
        return -1;

    UINT rd = 0;
    FRESULT res = f_read(fil, data, (UINT)size, &rd);
    if (res != FR_OK)
        return -1;
    return (int)rd;
}

int storage_file_seek(file_t *f, uint32_t offset)
{
    FIL *fil = (FIL *)f->handle;
    if (!fil)
        return -1;

    FRESULT res = f_lseek(fil, offset);
    if (res != FR_OK)
        return -1;
    return 0;
}

uint32_t storage_file_size(file_t *f)
{
    return f->size;
}

int storage_file_close(file_t *f)
{
    FIL *fil = (FIL *)f->handle;
    if (!fil)
        return -1;

    FRESULT res = f_close(fil);
    free(fil);
    f->handle = NULL;
    if (res != FR_OK)
        return -1;
    return 0;
}

int storage_file_delete(const char *name)
{
    FRESULT res = f_unlink(name);
    if (res != FR_OK)
    {
        LOG_ERROR("delete %s error[%d]", name, res);
        return -1;
    }
    return 0;
}

int storage_file_exists(const char *name)
{
    FILINFO fno;
    FRESULT res = f_stat(name, &fno);
    return (res == FR_OK) ? 1 : 0;
}
//<<----------------------
