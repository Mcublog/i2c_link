#ifndef APP_IO_TYPES_H
#define APP_IO_TYPES_H

/** @brief Тип сигнала на входе (результат дебаунсера) */
typedef enum
{
    EDGE_LOW_PLATO = 0, /**< низкий уровень (стабильный) */
    EDGE_RAISING, /**< фронт rising (с 0 на 1) */
    EDGE_FALLING, /**< фронт falling (с 1 на 0) */
    EDGE_HIGH_PLATO, /**< высокий уровень (стабильный) */
    EDGE_BOUNCE /**< дребезг (смесь 0 и 1) */
} egde_t;


#endif // APP_IO_TYPES_H