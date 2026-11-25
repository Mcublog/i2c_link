/**
 * @file usb_app.c
 * @brief Запуск USB-стека TinyUSB на плате fntester_pcb
 * @version 0.1
 * @date 2026-11-25
 *
 */
#include "usb/usb_app.h"

#include "main.h"
#include "tusb.h"

#include "app/system/systick.h"
#include "usb/tiny_usb_port.h"

#include "app/utils/idelay.h"
//>>---------------------- Log control
#define LOG_MODULE_NAME usb
#define LOG_MODULE_LEVEL (3)
#include "log_libs.h"
//<<----------------------

//>>---------------------- Local
/** @brief Пауза с отключённой линией D+ при старте, мс */
#define USB_REENUM_DELAY_MS (200)
//<<----------------------

//>>---------------------- Public
bool usb_app_init(void)
{
    // NOTE: ядро USB тактируется от PLL (48 МГц), настройка выполняется
    // в SystemClock_Config() до запуска приложения
    __HAL_RCC_USB_CLK_ENABLE();

    // NOTE: после сброса без снятия питания D+ поднимается слишком
    // быстро, хост не успевает зафиксировать отключение и сохраняет
    // прежнее состояние (устройство при этом уже не сконфигурировано).
    // Принудительно держим линию отключённой, чтобы гарантировать
    // повторную энумерацию.
    USB->BCDR &= ~USB_BCDR_DPPU;
    delay_ms(USB_REENUM_DELAY_MS);

    tiny_usb_port_init();

    if (!tusb_init())
    {
        LOG_ERROR("tusb_init failed");
        return false;
    }

    LOG_INFO("usb device started");
    return true;
}

void usb_app_poll(void)
{
    tud_task();
}

uint32_t tusb_time_millis_api(void)
{
    return (uint32_t)systick();
}
//<<----------------------
