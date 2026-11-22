#pragma once

#include "Serial.hpp"

/**
 * @brief Последовательный транспорт поверх USB VCP (CDC)
 */
class UsbSerial final : public Serial
{
  public:
    /**
     * @brief Инициализация транспорта и USB VCP
     *
     * @param ctl контекст последовательного транспорта
     * @return true - успешно
     */
    bool init(ios_ctl_t *ctl) override;

    /**
     * @brief Инициализация по имени порта (для USB не используется)
     *
     * @param portname имя порта
     * @param ctl контекст последовательного транспорта
     * @return true - успешно
     */
    bool init(const char *portname, ios_ctl_t *ctl) override;

    /**
     * @brief Проверка работоспособности
     *
     * @return true - транспорт готов
     */
    bool helth() override { return true; }

    /**
     * @brief Передача данных через USB VCP
     *
     * @param data данные
     * @param size размер данных
     * @return true - успешно
     */
    bool write(const uint8_t *data, uint32_t size) override;

    /**
     * @brief Чтение (не поддерживается, приём идёт через callback)
     *
     * @param succsess признак успешного чтения
     * @return uint8_t прочитанный байт
     */
    uint8_t read(bool *succsess) override;

    /**
     * @brief Регистрация обработчика приёма
     *
     * @param irq обработчик принятых данных
     */
    void register_irq(ios_irq_handler_t irq) override;
};