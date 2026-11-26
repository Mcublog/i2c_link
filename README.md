# i2c_link

[![CI](https://github.com/Mcublog/i2c_link/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/Mcublog/i2c_link/actions/workflows/ci.yml)

Мост **USB ↔ I2C** на плате **fntester_pcb** (STM32F072CB): превращает
компьютер в мастер шины I2C — чтение и запись регистров устройств,
сканирование адресов, восстановление зависшей шины.

Устройство составное и предоставляет два независимых интерфейса:

- **CDC VCP** — виртуальный COM-порт с собственным бинарным протоколом
  (COBS/R + CRC-16): произвольные транзакции, регистры размером 1 и 2
  байта с repeated start, диагностика шины и счётчики ошибок;
- **vendor-интерфейс i2c-tiny-usb** (VID:PID `1209:21C2`, обмен через
  EP0) — совместим с драйвером Linux `i2c-tiny-usb`, устройство видно
  как обычный I2C-адаптер `/dev/i2c-*`.

## Возможности

- команды протокола: `READ`, `WRITE`, `SCAN`, `INFO`, `PING`,
  `BUS_RECOVER`, `STATS`, настройка режима (`SET_MODE`,
  `SET_SLAVE_ADDR`, `GET_CONFIG`);
- режим **I2C slave**: внешний мастер обращается к устройству по
  заданному адресу, транзакции туннелируются хосту (`SLAVE_EVENT` /
  `SLAVE_DONE`) с clock stretching на время ответа;
- 7-битная адресация, payload до 128 байт, коды ошибок шины
  (`NACK_ADDR`, `NACK_DATA`, `TIMEOUT`, `BUS_ERROR`, `OVERFLOW` и др.);
- восстановление шины (BUS_RECOVER) и счётчики транзакций/ошибок;
- эмулятор прошивки на хосте: два I2C-устройства (`0x0C`, `0x50`) и
  транспорт через UNIX-сокет;
- хост-утилита `i2c-link-adapter`: мост «UNIX-сокет ↔ CDC» с
  прозрачным и семантическим режимами для моделирования slave-устройства
  (см. `utils/i2c-link-adapter/README.md`).

## Состав репозитория

- `app/` — платформонезависимая логика: кадрирование и протокол
  (`proto/`), команды шины (`i2c/`), мост i2c-tiny-usb (`bridge/`),
  интерфейсы подсистем.
- `bsp/` — реализация под плату: I2C2 на HAL, транспорт, USB-дескрипторы
  (`fntester_pcb/`), эмуляция для хоста (`desktop/`).
- `arch/` — критические секции, системное время, задержки
  (`desktop/`, `stm32f072/`).
- `targets/` — исполняемые цели: `fntester_pcb/` (прошивка),
  `emu/` (эмулятор).
- `test/` — юнит-тесты на хосте, имя цели — `utest_<name>`.
- `utils/` — хост-утилиты (в прошивку не входят): `i2c-link-adapter`
  — мост «UNIX-сокет ↔ CDC» для slave-режима (см.
  `utils/i2c-link-adapter/README.md`).
- `libs/` — TinyUSB, Ring-Buffer, cobs-c (git-сабмодули), serdev,
  ringfs, SoftTimer, RTT.
- `docs/` — документация и описания форматов.

Правила кода и подробная структура каталогов — в `AGENTS.md`.

## Сборка и тесты

Требования: CMake ≥ 3.22, `gcc/g++` для хост-сборки,
`arm-none-eabi-gcc/g++` для прошивки. После клонирования нужно
инициализировать сабмодули:

```sh
git submodule update --init --recursive
```

Хост-сборка (эмулятор, утилиты, тесты):

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Те же действия через `Makefile`:

```sh
make            # прошивка + эмулятор + утилиты + тесты
make firmwares  # прошивка (build/fw)
make emuls      # эмулятор (build/host)
make utils      # хост-утилиты (build/host)
make utest      # собрать и запустить тесты (build/host)
make clean      # очистить обе сборки
```

## Прошивка fntester_pcb

CubeMX-проект STM32F072CB (I2C2 на PB10/PB11, USB FS, RTC, TIM6/TIM7,
ADC + DMA) лежит в `targets/fntester_pcb/`; исходники подхватываются
CMake автоматически, после перегенерации CubeMX нужно перезапустить
конфигурацию.

```sh
cmake -S . -B build/fw -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
    -DCMAKE_CXX_COMPILER=arm-none-eabi-g++
cmake --build build/fw
```

Результат — `build/fw/targets/fntester_pcb/fntester_pcb` (+ `.hex`,
`.bin`, `.jlink`). USB-стек — TinyUSB; чек-лист настроек CubeMX и
описание транспорта — в `targets/fntester_pcb/README.md`.

## Протокол и эмулятор

Спецификация протокола host ↔ I2C — `docs/data/protocol.md`. Приём из
транспорта копится в кольцевом буфере и разбирается в основном цикле
`application()`; ответы выталкиваются в транспорт с повтором, если
передатчик занят.

Эмулятор принимает одно подключение по UNIX-сокету (по умолчанию
`/tmp/i2c_link.sock`, переопределяется переменной `I2C_LINK_EP`) и
эмулирует два I2C-устройства: `0x0C` и `0x50` (регистровый файл по
256 байт).

## Документация

- `AGENTS.md` — правила кода и структура проекта;
- `docs/data/protocol.md` — протокол host ↔ I2C;
- `targets/fntester_pcb/README.md` — прошивка и настройки CubeMX;
- `utils/i2c-link-adapter/README.md` — хост-адаптер slave-режима;
- `docs/` — остальная документация проекта.
