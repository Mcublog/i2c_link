# USB: TinyUSB + i2c-tiny-usb

Составное USB-устройство на стеке TinyUSB (сабмодуль `libs/tinyusb`,
тег 0.21.0):

| Интерфейс | Класс | Драйвер Linux | Назначение |
|---|---|---|---|
| IF0 | CDC (comm) | `cdc_acm` | `/dev/ttyACM*`, протокол моста (COBS/R + CRC16) |
| IF1 | CDC (data) | `cdc_acm` | данные виртуального COM-порта |
| IF2 | vendor `0xFF`, без эндпоинтов | `i2c_tiny_usb` | `/dev/i2c-N`, протокол i2c-tiny-usb |

IAD на CDC-функцию обязателен: он же защищает от ложной привязки
`i2c_tiny_usb` к CDC-интерфейсам (probe драйвера отвергает интерфейс
с IAD, у которого `bFunctionClass != 0xFF`).

## Файлы

- `tusb_config.h` — конфигурация стека (FS-device, CDC 256/256, vendor
  без FIFO и эндпоинтов);
- `usb_descriptors.c` — device/config/string дескрипторы; серийный номер
  формируется из 96-битного UID;
- `usb_app.c/.h` — запуск стека, `tud_task()` из главного цикла,
  `tusb_time_millis_api()` поверх `systick()`;
- `tiny_usb_port.c` — `tud_vendor_control_xfer_cb()`: разбор EP0-запросов
  и маппинг на `app/bridge/tiny_usb.c`;
- `../../../app/bridge/tiny_usb.c` — сам протокол, не зависит от USB и
  покрыт `test/tinyusb`.

Транспорт VCP живёт в `bsp/fntester_pcb/impl/transport.c`
(`tud_cdc_read/write`), его опрашивает `transport_poll()` из
`app/application.cpp`.

## Linux

Устройство представляется как `0x1209:0x21C2` (VID pid.codes для open
source hardware, PID не занят в опубликованном списке; при публикации
продукта PID нужно зарегистрировать на https://pid.codes).

Драйвер `i2c_tiny_usb` знает только `0x0403:0xc631` и `0x1c40:0x0534`,
поэтому наш VID/PID добавляется через `new_id`:

```
sudo install -m 755 scripts/i2c-tiny-usb-bind.sh \
     /usr/local/bin/i2c-tiny-usb-bind
sudo install -m 644 scripts/99-i2c-tiny-usb.rules \
     /etc/udev/rules.d/99-i2c-tiny-usb.rules
sudo udevadm control --reload-rules
```

Разово, без udev: `sudo scripts/i2c-tiny-usb-bind.sh 1209 21c2`.

После привязки появляется `/dev/i2c-N`, работают `i2cdetect`,
`i2cget`, `i2cset`, `i2ctransfer`.

## Особенности реализации

- **DTR не требуется.** Данные CDC копятся в TX FIFO и уходят, как только
  хост открывает порт. Это сохраняет поведение прежней прошивки на
  ST-стеке.
- **Гарантированная энумерация при старте.** На плате стоит внешний
  pull-up на D+, поэтому после сброса без снятия питания хост не видит
  отключения и сохраняет прежнее состояние (устройство при этом
  неконфигурировано: `cfg_num = 0`, эндпоинты закрыты). Поэтому перед
  запуском стека линия D+ (PA12) на 200 мс притягивается к земле как
  GPIO: хост фиксирует отключение и нумерует устройство заново.
- **Тактирование и NVIC настраиваются в `usb_app_init()`**, чтобы не
  зависеть от кода CubeMX для периферии USB.
- **Control-транзакции выполняются синхронно** в контексте `tud_task()`
  (главный цикл), поэтому блокирующие вызовы `HAL_I2C_*` допустимы.
- **Фаза 1: без repeated start.** Каждый `i2c_msg` — отдельная транзакция
  START…STOP; флаги `BEGIN/END` разбираются, но не используются.
- Для read при NACK отдаётся полный буфер `0xFF`, а NAK сообщается через
  `GET_STATUS` — иначе драйвер вернёт `-EIO` и `i2cdetect` может увидеть
  несуществующие адреса.

## CubeMX

В `fntester_pcb.ioc` мидлварь `USB_DEVICE` выключена, но сам
периферийный блок USB оставлен в режиме PCD (CubeMX генерирует
`Core/Src/usb.c` и `MX_USB_PCD_Init()`). Этот код для работы USB не
используется: TinyUSB инициализирует контроллер сам, а HAL_PCD лишь
включает тактирование и прерывание. Если в сборку попадёт
`MX_USB_PCD_Init()`, конфликта нет — `dcd_init()` всё равно
перенастроит периферию, pull-up до `tusb_init()` не поднимается.

После регенерации CubeMX важно, чтобы в `Core/Src/stm32f0xx_it.c`
остался вызов обработчика TinyUSB — он записан в USER CODE-блок и
переживает регенерацию:

```c
void USB_IRQHandler(void)
{
  /* USER CODE BEGIN USB_IRQn 0 */
  // NOTE: прерывание обслуживает стек TinyUSB (dcd_stm32_fsdev)
  dcd_int_handler(0);
  /* USER CODE END USB_IRQn 0 */
  ...
}
```

Если когда-нибудь понадобится полностью убрать USB из CubeMX — нужно
удалить `Mcu.IP9=USB`, `NVIC.USB_IRQn`, пины PA11/PA12 и
`RCC.*USB*`-параметры; тогда `USB_IRQHandler` придётся определить в
нашем коде, а тактирование USB уже настраивается в `usb_app_init()`.

## Проверено на железе

- INA219 (0x40) на I2C2 (PB10/PB11): `i2cdetect -y 8` показывает только
  0x40, фантомных адресов нет, время сканирования ~0.7 с;
- чтение: config = 0x399F (значение по умолчанию), bus voltage ~28.9 В,
  shunt voltage -40 мкВ без нагрузки — и через `i2cget ... w`, и через
  `i2ctransfer -y 8 w1@0x40 0x02 r2`;
- запись: `i2ctransfer -y 8 w3@0x40 0x05 0x10 0x00` (калибровка 0x1000)
  читается обратно корректно;
- регистровое чтение (write + STOP + read) работает в фазе 1, без
  repeated start.

**Внимание:** `i2cget ... w` отдаёт слово в SMBus-порядке (LSB first),
поэтому для INA219 (MSB first) значение выглядит байт-перевёрнутым:
0x399F читается как `0x9f39`. Для честного порядка используйте
`i2ctransfer`.

## Известные ограничения

- Linux-only: на Windows/macOS драйвера i2c-tiny-usb нет.
- `i2cdetect -y N` занимает ~0.5 с (одна control-транзакция на адрес).
- Долгая транзакция I2C (до 10 мс по `I2C_TIMEOUT_MS`) задерживает
  обслуживание USB, включая CDC.
