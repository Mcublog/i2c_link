# targets/fntester_pcb

Цель сборки прошивки для платы **fntester_pcb** на **STM32F072CB**
(Cortex-M0, 128 КБ flash, 16 КБ RAM). Имя цели сборки — `fntester_pcb`.

## Проект CubeMX

CubeMX-проект лежит в этом каталоге: `Core/`, `Drivers/`, `USB_DEVICE/`,
`Middlewares/`, `startup_stm32f072xb.s`, `STM32F072xx_FLASH.ld`.
Сборка подхватывает исходники автоматически (`file(GLOB_RECURSE)`),
поэтому после перегенерации CubeMX нужно перезапустить CMake:

```sh
cmake -S . -B build -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
    -DCMAKE_CXX_COMPILER=arm-none-eabi-g++
cmake --build build -j
```

## Что сконфигурировано в CubeMX

- **MCU:** STM32F072CB (LQFP48), HSE + PLL, SYSCLK 48 МГц.
- **I2C2:** PB10 (SCL) / PB11 (SDA) — `hi2c2`.
- **USB FS:** Device, класс CDC (Virtual COM Port) — `hUsbDeviceFS`.
- **RTC:** включён (HSE/32), календарь.
- **TIM6 / TIM7:** 1 МГц (prescaler 47) — TIM6 используется для
  `delay_us()` через `arch/stm32f072/impl/tick_timer.c`.
- **ADC:** PA0 + DMA1_Channel1 (circular).
- **GPIO:** см. `Core/Src/gpio.c`; светодиод — `LED_GREEN_Pin`
  (PA9), используется в `bsp/fntester_pcb/io/gpio.cpp`.

## Интеграция с шаблоном

Правки в USER CODE-секциях (безопасны при перегенерации CubeMX):

- `Core/Src/main.c` — инициализация TIM6 (`tt_init`), программных
  таймеров (`Timer_Init`) и запуск `application()`.
- `Core/Src/stm32f0xx_it.c` — `systick_handler()` в `SysTick_Handler`.
- `USB_DEVICE/App/usbd_cdc_if.c` — принятые по VCP данные передаются в
  `usb_vcp_irq_handler()`.

USB VCP транспорт:

- `arch/stm32f072/hw/usb_vcp.h` + `arch/stm32f072/io/usb_vcp.c` —
  низкоуровневый слой (`CDC_Transmit_FS`, приём из `CDC_Receive_FS`);
- `arch/stm32f072/UsbSerial.hpp/.cpp` — реализация интерфейса
  `Serial` из `libs/serdev`.

## UART

В текущей конфигурации CubeMX **USART не включён**, поэтому
`arch/stm32f072/io/uart.c` компилируется в пустышку (под
`HAL_UART_MODULE_ENABLED`). Чтобы включить UART-транспорт, добавь USARTx
в CubeMX (Asynchronous + NVIC global interrupt) и перегенерируй проект.

## Ограничения платформы

- У STM32F072 **нет Ethernet MAC**. «ETH» в схеме serial <-> i2c
  реализуется только внешним модулем (W5500, ESP32 и т.п.) по SPI/UART.
- USB FS (VCP), UART, CAN — на кристалле.
- Cortex-M0: нет FPU, нет счётчика циклов DWT.