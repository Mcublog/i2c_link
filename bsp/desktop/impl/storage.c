/**
 * @file storage.c
 * @author Viacheslav (viacheslav@mcublog.ru)
 * @brief Файловая система для десктопной сборки (хостовые файлы)
 * @version 0.1
 * @date 2026-08-02
 *
 * @copyright Viacheslav mcublog (c) 2026
 *
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "app/storage/storage.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME fsdesk
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
#define FS_BASE_DIR "/tmp/fn"
#define FS_PATH_MAX (128)
//<<----------------------

//>>---------------------- Private
static void make_path(const char *name, char *path, size_t size)
{
    snprintf(path, size, "%s/%s", FS_BASE_DIR, name);
}
//<<----------------------

//>>---------------------- Public
int storage_init(void)
{
    mkdir(FS_BASE_DIR, 0700);
    return 0;
}

int storage_file_create(const char *name, file_t *f)
{
    char path[FS_PATH_MAX];
    make_path(name, path, sizeof(path));

    FILE *fp = fopen(path, "wb");
    if (!fp)
    {
        LOG_ERROR("create %s error", path);
        return -1;
    }

    f->handle = fp;
    f->size = 0;
    return 0;
}

int storage_file_open(const char *name, file_t *f)
{
    char path[FS_PATH_MAX];
    make_path(name, path, sizeof(path));

    FILE *fp = fopen(path, "r+b");
    if (!fp)
    {
        LOG_ERROR("open %s error", path);
        return -1;
    }

    f->handle = fp;
    fseek(fp, 0, SEEK_END);
    f->size = (uint32_t)ftell(fp);
    rewind(fp);
    return 0;
}

int storage_file_write(file_t *f, const void *data, size_t size)
{
    FILE *fp = (FILE *)f->handle;
    if (!fp)
        return -1;

    size_t written = fwrite(data, 1, size, fp);
    if (written != size)
    {
        LOG_ERROR("write error");
        return -1;
    }

    f->size += (uint32_t)written;
    return 0;
}

int storage_file_read(file_t *f, void *data, size_t size)
{
    FILE *fp = (FILE *)f->handle;
    if (!fp)
        return -1;

    size_t rd = fread(data, 1, size, fp);
    return (int)rd;
}

int storage_file_seek(file_t *f, uint32_t offset)
{
    FILE *fp = (FILE *)f->handle;
    if (!fp)
        return -1;
    return fseek(fp, offset, SEEK_SET);
}

uint32_t storage_file_size(file_t *f)
{
    return f->size;
}

int storage_file_close(file_t *f)
{
    FILE *fp = (FILE *)f->handle;
    if (!fp)
        return -1;
    fclose(fp);
    f->handle = NULL;
    return 0;
}

int storage_file_delete(const char *name)
{
    char path[FS_PATH_MAX];
    make_path(name, path, sizeof(path));
    if (remove(path) != 0)
    {
        LOG_ERROR("delete %s error", path);
        return -1;
    }
    return 0;
}

int storage_file_exists(const char *name)
{
    char path[FS_PATH_MAX];
    make_path(name, path, sizeof(path));

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    fclose(fp);
    return 1;
}
//<<----------------------
