/**
 * @file usb_descriptors.c
 * @brief Дескрипторы USB-устройства: CDC (VCP) + vendor (i2c-tiny-usb)
 * @version 0.1
 * @date 2026-11-25
 *
 * Устройство составное: два интерфейса CDC (виртуальный COM-порт с
 * протоколом моста) и vendor-интерфейс без эндпоинтов, обмен с которым
 * идёт только через EP0 (протокол i2c-tiny-usb).
 */
#include "tusb.h"

//>>---------------------- Local
// NOTE: VID 0x1209 принадлежит pid.codes (open source hardware).
// PID 0x21C2 на момент написания не занят в опубликованном списке
// https://pid.codes/1209/ - при публикации продукта PID нужно
// зарегистрировать.
/** @brief Идентификатор производителя (pid.codes) */
#define USB_VID (0x1209)
/** @brief Идентификатор продукта */
#define USB_PID (0x21C2)

/** @brief Номер строки: производитель */
#define STR_IDX_MANUFACTURER (1)
/** @brief Номер строки: продукт */
#define STR_IDX_PRODUCT (2)
/** @brief Номер строки: серийный номер */
#define STR_IDX_SERIAL (3)

/** @brief Номер интерфейса CDC (control) */
#define ITF_NUM_CDC (0)
/** @brief Номер интерфейса CDC (data) */
#define ITF_NUM_CDC_DATA (1)
/** @brief Номер vendor-интерфейса */
#define ITF_NUM_VENDOR (2)
/** @brief Количество интерфейсов в конфигурации */
#define ITF_NUM_TOTAL (3)

/** @brief Эндпоинт уведомлений CDC */
#define EPNUM_CDC_NOTIF (0x82)
/** @brief Эндпоинт данных CDC (OUT) */
#define EPNUM_CDC_OUT (0x01)
/** @brief Эндпоинт данных CDC (IN) */
#define EPNUM_CDC_IN (0x81)

/** @brief Длина дескриптора vendor-интерфейса, байт */
#define VENDOR_ITF_DESC_LEN (9)
/** @brief Полная длина конфигурации, байт */
#define CONFIG_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + VENDOR_ITF_DESC_LEN)

/** @brief Вместимость строкового дескриптора, символов */
#define STR_DESC_MAX_CHARS (32)
/** @brief Размер строкового дескриптора, байт */
#define STR_DESC_MAX_BYTES (2 + 2 * STR_DESC_MAX_CHARS)
/** @brief Количество элементов буфера строкового дескриптора */
#define STR_DESC_MAX_WORDS (STR_DESC_MAX_BYTES / 2)

/** @brief Код языка (английский, США) */
#define LANG_ID_EN_US (0x0409)
/** @brief Длина серийного номера в символах */
#define SERIAL_CHARS (24)

/** @brief Дескриптор устройства */
static const tusb_desc_device_t kDeviceDesc = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0200,
    .iManufacturer = STR_IDX_MANUFACTURER,
    .iProduct = STR_IDX_PRODUCT,
    .iSerialNumber = STR_IDX_SERIAL,
    .bNumConfigurations = 0x01,
};

/** @brief Дескриптор конфигурации */
static const uint8_t kConfigDesc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80,
                          100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 0, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT,
                       EPNUM_CDC_IN, 64),
    /* Vendor-интерфейс i2c-tiny-usb: обмен только через EP0 */
    VENDOR_ITF_DESC_LEN, TUSB_DESC_INTERFACE, ITF_NUM_VENDOR, 0, 0,
    TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, 0x00,
};

/** @brief Буфер строкового дескриптора */
static uint16_t m_str_desc[STR_DESC_MAX_WORDS];

//>>---------------------- Private
/**
 * @brief Заполнить строковый дескриптор ASCII-строкой
 *
 * @param str исходная строка
 * @param max_chars вместимость буфера в символах
 * @return uint16_t длина дескриптора в байтах
 */
static uint16_t fill_string(const char *str, uint16_t max_chars)
{
    uint16_t count = 0;

    while ((str[count] != '\0') && (count < max_chars))
    {
        m_str_desc[1 + count] = (uint16_t)str[count];
        count++;
    }

    uint16_t size = (uint16_t)(2 + 2 * count);
    m_str_desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | size);
    return size;
}

/**
 * @brief Получить серийный номер из 96-битного UID микроконтроллера
 *
 * @param buf буфер на SERIAL_CHARS + 1 символов
 */
static void serial_from_uid(char *buf)
{
    // NOTE: STM32F072 - 96-битный уникальный идентификатор
    const volatile uint32_t *uid = (const volatile uint32_t *)0x1FFFF7ACUL;
    static const char kHex[] = "0123456789ABCDEF";
    uint32_t offset = 0;

    for (uint32_t word = 0; word < 3; word++)
    {
        for (int shift = 28; shift >= 0; shift -= 4)
        {
            uint32_t value = (uid[word] >> (uint32_t)shift) & 0x0Fu;
            buf[offset++] = kHex[value];
        }
    }
    buf[SERIAL_CHARS] = '\0';
}
//<<----------------------

//>>---------------------- Public
uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&kDeviceDesc;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return kConfigDesc;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    switch (index)
    {
    case 0:
        m_str_desc[1] = LANG_ID_EN_US;
        m_str_desc[0] =
            (uint16_t)((TUSB_DESC_STRING << 8) | (2 + 2 * 1));
        return m_str_desc;

    case STR_IDX_MANUFACTURER:
        fill_string("STMicroelectronics", STR_DESC_MAX_CHARS);
        return m_str_desc;

    case STR_IDX_PRODUCT:
        fill_string("STM32 Virtual ComPort", STR_DESC_MAX_CHARS);
        return m_str_desc;

    case STR_IDX_SERIAL:
    {
        char serial[SERIAL_CHARS + 1];
        serial_from_uid(serial);
        fill_string(serial, SERIAL_CHARS);
        return m_str_desc;
    }

    default:
        return NULL;
    }
}
//<<----------------------
