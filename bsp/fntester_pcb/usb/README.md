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
- **Гарантированная энумерация при старте.** Перед запуском стека линия
  D+ удерживается отключённой 200 мс (`USB->BCDR`), иначе после сброса
  без снятия питания хост может не заметить переподключение и сохранить
  прежнее состояние (устройство окажется неконфигурированным:
  `cfg_num = 0`, эндпоинты закрыты). При отладке через J-Link всё равно
  надёжнее переподключить кабель.
- **Control-транзакции выполняются синхронно** в контексте `tud_task()`
  (главный цикл), поэтому блокирующие вызовы `HAL_I2C_*` допустимы.
- **Фаза 1: без repeated start.** Каждый `i2c_msg` — отдельная транзакция
  START…STOP; флаги `BEGIN/END` разбираются, но не используются.
- Для read при NACK отдаётся полный буфер `0xFF`, а NAK сообщается через
  `GET_STATUS` — иначе драйвер вернёт `-EIO` и `i2cdetect` может увидеть
  несуществующие адреса.

## CubeMX

USB-стек больше не генерируется CubeMX: `USB_DEVICE` в `fntester_pcb.ioc`
надо выключить. Пока это не сделано, после регенерации нужно:

1. удалить `targets/fntester_pcb/USB_DEVICE/` и
   `targets/fntester_pcb/Middlewares/ST/STM32_USB_Device_Library/`;
2. убрать из `Core/Src/main.c` вызов `MX_USB_DEVICE_Init()` и
   `#include "usb_device.h"`;
3. оставить в `Core/Src/stm32f0xx_it.c` вызов `dcd_int_handler(0)` в
   `USB_IRQHandler`;
4. в `Core/Inc/stm32f0xx_hal_conf.h` держать
   `HAL_PCD_MODULE_ENABLED` выключенным.

## Известные ограничения

- Linux-only: на Windows/macOS драйвера i2c-tiny-usb нет.
- `i2cdetect -y N` занимает ~0.5 с (одна control-транзакция на адрес).
- Долгая транзакция I2C (до 10 мс по `I2C_TIMEOUT_MS`) задерживает
  обслуживание USB, включая CDC.
