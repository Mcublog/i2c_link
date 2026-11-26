#ifndef APP_IO_ACTIVITY_H
#define APP_IO_ACTIVITY_H

/** @brief Выдержка свечения LED1 после завершения транзакции, мс */
#define ACTIVITY_HOLD_MS (30)

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Инициализировать индикатор активности шины
     *
     * Сбрасывает состояние и выключает LED1.
     */
    void activity_init(void);

    /**
     * @brief Отметить начало транзакции
     *
     * Включает LED1. Может вызываться как из главного цикла, так и
     * из обработчика прерывания шины.
     */
    void activity_begin(void);

    /**
     * @brief Отметить завершение транзакции
     *
     * LED1 остаётся включённым ещё ACTIVITY_HOLD_MS и гаснет в
     * activity_poll(), чтобы короткие транзакции были заметны.
     */
    void activity_end(void);

    /**
     * @brief Обслужить индикатор активности
     *
     * Вызывается регулярно из главного цикла: гасит LED1, когда
     * транзакций нет и выдержка после последней истекла.
     */
    void activity_poll(void);

#ifdef __cplusplus
}
#endif

#endif // APP_IO_ACTIVITY_H
