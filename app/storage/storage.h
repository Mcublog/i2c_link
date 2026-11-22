#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct file
{
    void *handle; /**< описатель файла */
    uint32_t size; /**< текущий размер файла */
} file_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализация файловой системы
     *
     * @return int 0 - успешно, иначе ошибка
     */
    int storage_init(void);

    /**
     * @brief Создать файл
     *
     * @param name имя файла
     * @param f описатель открытого файла
     * @return int 0 - успешно, иначе ошибка
     */
    int storage_file_create(const char *name, file_t *f);

    /**
     * @brief Открыть существующий файл
     *
     * @param name имя файла
     * @param f описатель открытого файла
     * @return int 0 - успешно, иначе ошибка
     */
    int storage_file_open(const char *name, file_t *f);

    /**
     * @brief Записать данные (с текущей позиции)
     *
     * @param f описатель файла
     * @param data данные
     * @param size размер данных
     * @return int 0 - успешно, иначе ошибка
     */
    int storage_file_write(file_t *f, const void *data, size_t size);

    /**
     * @brief Прочитать данные (с текущей позиции)
     *
     * @param f описатель файла
     * @param data буфер
     * @param size размер данных
     * @return int количество прочитанных байт, иначе ошибка
     */
    int storage_file_read(file_t *f, void *data, size_t size);

    /**
     * @brief Установить позицию в файле от начала
     *
     * @param f описатель файла
     * @param offset смещение от начала
     * @return int 0 - успешно, иначе ошибка
     */
    int storage_file_seek(file_t *f, uint32_t offset);

    /**
     * @brief Получить размер файла
     *
     * @param f описатель файла
     * @return uint32_t размер файла
     */
    uint32_t storage_file_size(file_t *f);

    /**
     * @brief Закрыть файл
     *
     * @param f описатель файла
     * @return int 0 - успешно, иначе ошибка
     */
    int storage_file_close(file_t *f);

    /**
     * @brief Удалить файл
     *
     * @param name имя файла
     * @return int 0 - успешно, иначе ошибка
     */
    int storage_file_delete(const char *name);

    /**
     * @brief Проверить существование файла
     *
     * @param name имя файла
     * @return int 1 - существует, 0 - нет
     */
    int storage_file_exists(const char *name);

#ifdef __cplusplus
}
#endif
