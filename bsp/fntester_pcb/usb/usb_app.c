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

//>>---------------------- Private
/**
 * @brief Удерживать линию D+ в нуле, имитируя отключение от шины
 *
 * На плате установлен внешний pull-up на D+, поэтому обнуления
 * USB_BCDR_DPPU недостаточно: притягиваем D+ (PA12) к земле, чтобы хост
 * гарантированно зафиксировал отключение и заново пронумеровал
 * устройство.
 *
 * @param ms длительность удержания, мс
 */
static void hold_disconnect(uint32_t ms)
{
    GPIO_InitTypeDef io = {0};

    io.Pin = GPIO_PIN_12;
    io.Mode = GPIO_MODE_OUTPUT_PP;
    io.Pull = GPIO_NOPULL;
    io.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &io);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

    delay_ms(ms);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12);
}

/**
 * @brief Настроить тактирование USB (источник - PLL, 48 МГц)
 *
 * @return true - настройка выполнена
 */
static bool clock_init(void)
{
    RCC_PeriphCLKInitTypeDef periph = {0};

    periph.PeriphClockSelection = RCC_PERIPHCLK_USB;
    periph.UsbClockSelection = RCC_USBCLKSOURCE_PLL;

    return HAL_RCCEx_PeriphCLKConfig(&periph) == HAL_OK;
}
//<<----------------------

//>>---------------------- Public
bool usb_app_init(void)
{
    // NOTE: тактирование и NVIC настраиваем сами, чтобы не зависеть от
    // кода, который генерирует CubeMX для периферии USB
    if (!clock_init())
    {
        LOG_ERROR("usb clock init failed");
        return false;
    }

    __HAL_RCC_USB_CLK_ENABLE();
    HAL_NVIC_SetPriority(USB_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USB_IRQn);

    // NOTE: после сброса без снятия питания D+ поднимается слишком
    // быстро, хост не успевает зафиксировать отключение и сохраняет
    // прежнее состояние (устройство при этом уже не сконфигурировано).
    // Принудительно держим линию отключённой, чтобы гарантировать
    // повторную энумерацию.
    USB->BCDR &= ~USB_BCDR_DPPU;
    hold_disconnect(USB_REENUM_DELAY_MS);

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
