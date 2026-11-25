/**
 * @file tusb_config.h
 * @brief Конфигурация TinyUSB для платы fntester_pcb (STM32F072)
 * @version 0.1
 * @date 2026-11-25
 *
 */
#ifndef BSP_FNTESTER_PCB_USB_TUSB_CONFIG_H
#define BSP_FNTESTER_PCB_USB_TUSB_CONFIG_H

//>>---------------------- Local
/** @brief Порт 0: полноскоростное USB-устройство */
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
/** @brief Микроконтроллер (порт stm32_fsdev) */
#define CFG_TUSB_MCU OPT_MCU_STM32F0
/** @brief Стек работает без RTOS, из главного цикла */
#define CFG_TUSB_OS OPT_OS_NONE
/** @brief Отладка TinyUSB отключена */
#define CFG_TUSB_DEBUG (0)

/** @brief Максимальный размер пакета EP0 */
#define CFG_TUD_ENDPOINT0_SIZE (64)

/** @brief Количество CDC-интерфейсов (виртуальный COM-порт) */
#define CFG_TUD_CDC (1)
/** @brief Размер буфера приёма CDC, байт */
#define CFG_TUD_CDC_RX_BUFSIZE (256)
/** @brief Размер буфера передачи CDC, байт */
#define CFG_TUD_CDC_TX_BUFSIZE (256)
/** @brief Размер пакета эндпоинтов CDC */
#define CFG_TUD_CDC_EP_BUFSIZE (64)

/** @brief Количество vendor-интерфейсов (i2c-tiny-usb) */
#define CFG_TUD_VENDOR (1)
/** @brief Vendor-интерфейс не имеет эндпоинтов, FIFO не нужны */
#define CFG_TUD_VENDOR_RX_BUFSIZE (0)
/** @brief Vendor-интерфейс не имеет эндпоинтов, FIFO не нужны */
#define CFG_TUD_VENDOR_TX_BUFSIZE (0)
/** @brief Размер пакета неиспользуемого эндпоинта (только для буферов) */
#define CFG_TUD_VENDOR_RX_EPSIZE (8)
/** @brief Размер пакета неиспользуемого эндпоинта (только для буферов) */
#define CFG_TUD_VENDOR_TX_EPSIZE (8)
//<<----------------------

#endif // BSP_FNTESTER_PCB_USB_TUSB_CONFIG_H
