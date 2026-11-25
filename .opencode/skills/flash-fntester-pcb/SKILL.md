---
name: flash-fntester-pcb
description: >-
  Прошивка платы fntester_pcb (STM32F072CB) через J-Link по JLink-скрипту,
  сгенерированному CMake из cmake/burn.jlink.in. Используй, когда нужно
  залить новую прошивку на плату. Ключевые слова: прошить, прошивка,
  flash, burn, залить прошивку, JLink, JLinkExe, fntester_pcb, F072.
---

# flash-fntester-pcb

Прошивка платы fntester_pcb (STM32F072CB, Cortex-M0) через отладчик
J-Link PLUS по SWD. CMake генерирует готовый JLink-скрипт из шаблона
`cmake/burn.jlink.in`, остаётся только пересобрать и выполнить его через
`JLinkExe`.

## Предусловия

- J-Link PLUS подключён к USB (lsusb: `1366:0101 SEGGER J-Link PLUS`).
- Плата fntester_pcb питается.
- Проект собран с arm-none-eabi (`build/targets/fntester_pcb/`).
  Если сборки нет или она устарела — сначала пересобери.

## Как работает прошивка

- `build_jlink_script()` (`cmake/utils.cmake:13`) из шаблона
  `cmake/burn.jlink.in` формирует скрипт
  `build/targets/fntester_pcb/fntester_pcb.jlink` и копирует hex в
  `build/output/fntester_pcb_v<версия>.hex`.
- Версия прошивки берётся из `project(i2c_link VERSION ...)`
  (`CMakeLists.txt`) — путь к hex запекается в скрипт **на момент
  сборки**. Поэтому после изменения версии или кода обязательна
  пересборка.
- J-Link device: `STM32F072CB` (задаётся в
  `targets/fntester_pcb/build.cmake`, вызов `build_jlink_script`).
- Команды скрипта: `speed 4000`, `Device STM32F072CB`, `Con`
  (подключение), `r` (reset), `h` (halt), `loadfile <hex>`
  (стирание + запись), `r`, `h`, `g` (старт), `q` (выход). Скрипт
  завершается сам.
- `JLinkExe`: `/usr/bin/JLinkExe` -> `/opt/SEGGER/JLink_V786/JLinkExe`
  (V7.86).

## Helper-скрипт

В проекте есть обёртка `scripts/flash.sh`. Она находит
скрипт `.jlink` в `build/`, запускает `JLinkExe` с таймаутом, сохраняет
лог и проверяет успех прошивки.

    scripts/flash.sh -h        # справка

Параметры:
- `-b DIR` — каталог сборки (по умолчанию `build/`);
- `-s SN`  — serial J-Link для `-SelectEmuBySN` (нужен при нескольких
  отладчиках; подключён один: serial 69665469);
- `-o FILE` — сохранить вывод JLinkExe в FILE;
- `-h` — справка.

## Сценарии

### Пересобрать и прошить

    cmake --build build -j
    scripts/flash.sh

Пересборка обязательна: она регенерирует `.jlink` с актуальным путём к
hex и версией.

### Прошить без пересборки (сборка свежая)

    scripts/flash.sh -o flash.log

### Выбрать другой каталог сборки

    scripts/flash.sh -b /path/to/build

### Несколько J-Link — указать serial

    scripts/flash.sh -s 69665469

### Запустить JLinkExe напрямую

    JLinkExe -CommanderScript build/targets/fntester_pcb/fntester_pcb.jlink

## Анализ вывода

- Успех: строка `O.K.` (например, `Downloading file ... O.K.`), затем
  `g`/`q`. Код возврата скрипта 0.
- Ищи `ERROR:` / `Failed` / `Cannot connect` — проблема подключения или
  файла.
- `Downloading file: ... done.` — hex найден и записан.
- Полезно сверить в логе путь к hex с фактическим
  `build/output/fntester_pcb_v<версия>.hex`.

## Неисправности

- `Cannot connect` / `ERROR: Could not connect` — J-Link занят другим
  процессом (JLinkExe, JLinkRTTViewer, rtt-console). Закрой лишние
  процессы и повтори.
- Подключено несколько J-Link — добавь `-s <serial>`.
- `ERROR: Could not find file ...` — путь к hex в `.jlink` устарел или
  hex удалён; пересобери проект.
- Нет платы на SWD — проверь питание и кабель.
- Скрипт завершился по таймауту без `O.K.` — прошивка не подтверждена;
  посмотри лог.

## Ограничения

- Прошивка сбрасывает МК (`r` до и после записи, `g` запускает ядро).
- `loadfile` стирает и перезаписывает flash целиком (128 КБ у F072CB).
- `scripts/flash.sh` не пересобирает проект сам.
