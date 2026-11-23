# i2c_link

Шаблонный репозиторий для разработки встроенного ПО (embedded) на C/C++.
Проект — мост между последовательными интерфейсами (UART / USB VCP / внешний
Ethernet) и I2C на плате **fntester_pcb** (STM32F072CB).

Обмен между хостом и I2C идёт по бинарному протоколу (COBS/R + CRC-16):
спецификация — `docs/data/protocol.md`.

Содержит готовую структуру проекта и систему сборки на **CMake** для трёх
типов целей:

- **Desktop** — хостовая сборка для разработки и юнит-тестов;
- **Emulator** (`targets/emu`) — эмуляция прошивки на хосте;
- **Firmware** (`targets/fntester_pcb`) — прошивка под плату fntester_pcb
  (STM32F072CB, CubeMX + HAL).

Сторонние библиотеки подключены как git-сабмодули:

- `libs/Ring-Buffer` — кольцевой буфер (Mcublog);
- `libs/cobs-c` — COBS/COBS-R (cmcqueen).

После клонирования их нужно инициализировать:

```sh
git clone --recurse-submodules <repo>
# или, если репозиторий уже склонирован:
git submodule update --init --recursive
```

## Структура проекта

```
i2c_link/
├── CMakeLists.txt        # корневой CMake
├── Makefile              # обёртка над CMake (полезные цели)
├── app/                  # платформонезависимая логика и интерфейсы
├── bsp/                  # реализация под конкретную плату/платформу
├── arch/                 # низкоуровневые модули (крит. секции, таймеры, delay)
├── cmake/                # вспомогательные CMake-модули
├── libs/                 # сторонние/разделяемые библиотеки
├── resources/            # ресурсы: SVD, сгенерированные данные, даташиты
├── docs/                 # документация
├── scripts/              # хост-скрипты (не входят в прошивку)
├── targets/              # исполняемые цели сборки
│   ├── emu/              # эмулятор на хосте
│   └── fntester_pcb/     # прошивка под STM32F072CB
├── test/                 # юнит-тесты (desktop)
├── hwtest/               # тесты на железе (выключены по умолчанию)
└── utils/                # хост-утилиты
    └── hello/            # пример-заготовка утилиты
```

### Каталоги подробнее

- **`app/`** — только платформонезависимый код: интерфейсы подсистем
  (`storage.h`, `igpio.h`, `systime.h` и т.д.) и логика приложения.
  Никаких зависимостей от железа или ОС. Ключевые подсистемы:
  - `app/proto/` — кадрирование и канальный уровень протокола
    (`proto.h`, `frame.*`, `link.*`);
  - `app/i2c/` — интерфейс шины (`ii2c.h`) и обработчики команд
    (`i2c_service.*`);
  - `app/io/itransport.h` — интерфейс транспорта (USB VCP / сокет).
- **`bsp/`** — реализации под конкретную плату/платформу. Интерфейс
  объявляется в `app/`, реализация — в `bsp/<платформа>/`.
  - `bsp/fntester_pcb/impl/i2c.cpp` — I2C2 на HAL и восстановление шины;
  - `bsp/fntester_pcb/impl/transport.c` — USB VCP;
  - `bsp/desktop/impl/` — эмуляция I2C и транспорт через UNIX-сокет.
- **`arch/`** — низкоуровневые модули (критические секции, эмуляция
  времени, задержки). `arch/desktop/` — хост, `arch/stm32f072/` — МК.
- **`test/`** — юнит-тесты, собираются и запускаются на хосте. Один тест —
  свой подкаталог, имя цели — `utest_<name>` (например `utest_smoke`).
- **`utils/`** — хост-утилиты, не входят в прошивку. Каждая утилита —
  свой подкаталог.
- **`libs/`** — сторонние/разделяемые библиотеки. Их источники и
  include-пути описываются в `libs/libs.cmake`. Абстракция
  последовательного транспорта (`Serial`, `ios_ctl_t`) — `libs/serdev`.

## Быстрый старт

Зависимости: **CMake ≥ 3.22**, компилятор **gcc/g++** (desktop) и
**arm-none-eabi-gcc** (прошивка).

Для разработки (desktop + эмулятор + тесты):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Или через `Makefile`:

```sh
make                    # прошивка + эмулятор + утилиты + тесты
make utest              # собрать и запустить все тесты
make emuls              # собрать эмулятор
make utils              # собрать хост-утилиты
make clean              # очистить сборку
```

## Прошивка fntester_pcb

CubeMX-проект для STM32F072CB лежит в `targets/fntester_pcb/` (toolchain
Makefile): I2C2, USB CDC (VCP), RTC, TIM6/TIM7, ADC + DMA. Исходники
CubeMX (`Core/`, `Drivers/`, `USB_DEVICE/`, `Middlewares/`, startup, `.ld`)
подхватываются CMake автоматически; после перегенерации CubeMX нужно
перезапустить CMake.

USB VCP транспорт: приложение работает через `app/io/itransport.h`,
реализация для платы — `bsp/fntester_pcb/impl/transport.c` поверх
`usb_vcp_*`. Абстракция `Serial` из `libs/serdev` и
`arch/stm32f072/UsbSerial.*` остаются альтернативным транспортом.
Подробности и чек-лист настроек CubeMX — в `targets/fntester_pcb/README.md`.

Если каталог `targets/fntester_pcb/Core/Src/main.c` отсутствует, цель
прошивки не создаётся, а `make firmwares` выводит предупреждение.

## Цели сборки

Из `build/` (смотри `make -C build help`):

| Цель                     | Описание                              |
|--------------------------|---------------------------------------|
| `emu`                    | эмулятор прошивки (сервер)            |
| `fntester_pcb`           | прошивка fntester_pcb (+ `.hex`/`.bin`) |
| `hello`                  | хост-утилита (пример)                 |
| `utest_smoke`            | простейший юнит-тест                  |
| `utest_proto`            | тесты протокола host <-> I2C          |
| `dbg_sandbox`            | песочница для экспериментов           |

## Протокол и эмулятор

Обмен хост <-> I2C описан в `docs/data/protocol.md`: COBS/R с
терминатором `0x00`, CRC-16/CCITT-FALSE, команды `READ`, `WRITE`, `SCAN`,
`INFO`, `PING`, `BUS_RECOVER`, `STATS`.

Приём из транспорта копится в кольцевом буфере (`libs/Ring-Buffer`) и
разбирается в основном цикле `application()`; ответы складываются в
кольцевой буфер передачи и выталкиваются с повтором, если транспорт занят.

Эмулятор принимает одно подключение по UNIX-сокету (по умолчанию
`/tmp/i2c_link.sock`, переопределяется переменной `I2C_LINK_EP`) и
эмулирует два I2C-устройства: `0x0C` и `0x50` (регистровый файл по
256 байт).

## Как добавить

### Новый юнит-тест

1. Создай каталог `test/<name>/`.
2. Добавь туда `<name>_test.cpp` с `main()` и `CMakeLists.txt`:

   ```cmake
   include(${PROJECT_SOURCE_DIR}/test/build.cmake)
   set(CMAKE_C_COMPILER gcc)
   set(CMAKE_CXX_COMPILER g++)
   test_target_with_main_entry("utest_<name>" "" "" "")
   ```

3. Добавь каталог в `test/CMakeLists.txt`.
4. Перезапусти `cmake -S . -B build` (исходники собираются через `GLOB`).

### Новую плату

1. Создай `bsp/<плата>/impl/` и `arch/<архитектура>/`.
2. Реализуй интерфейсы из `app/` (например `app/storage/storage.h`).
3. Добавь цель сборки в `targets/`.

### Новую хост-утилиту

Создай `utils/<name>/` с `CMakeLists.txt` и `main.cpp`, добавь каталог
в `utils/CMakeLists.txt`. Утилита не входит в прошивку.

### Новую библиотеку в `libs/`

Добавь её источники и include-пути в `libs/libs.cmake`.

## Тесты на железе

Каталог `hwtest/` предназначен для тестов, требующих реальной платы.
По умолчанию он **не входит** в сборку (`add_subdirectory(hwtest)`
закомментирован в корневом `CMakeLists.txt`). Раскомментируйте строку,
чтобы включить.

## Документация

- `AGENTS.md` — инструкции для ИИ-агентов (структура, правила, стиль).
- `docs/` — документация проекта.
- `resources/` — файлы ресурсов.