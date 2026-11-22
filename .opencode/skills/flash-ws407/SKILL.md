---
name: flash-ws407
description: >-
  Прошивка платы ws407 (STM32F407VE) через J-Link по JLink-скрипту,
  сгенерированному CMake из cmake/burn.jlink.in. Используй, когда нужно
  залить новую прошивку на плату. Ключевые слова: прошить, прошивка,
  flash, burn, залить прошивку, JLink, JLinkExe, ws407.
---

# flash-ws407

Прошивка платы ws407 (STM32F407VE) через отладчик J-Link PLUS по SWD.
CMake генерирует готовый JLink-скрипт из шаблона `cmake/burn.jlink.in`,
остаётся только пересобрать и выполнить его через `JLinkExe`.

## Предусловия

- J-Link PLUS подключён к USB (lsusb: `1366:0101 SEGGER J-Link PLUS`).
- Плата ws407 питается (при питании только через J-Link убедись, что
  этого достаточно для прошивки).
- Проект собран (`build/` существует). Если сборки нет или она устарела —
  сначала пересобери.

## Как работает прошивка

- `build_jlink_script()` (`cmake/utils.cmake:13`) из шаблона
  `cmake/burn.jlink.in` формирует скрипт
   `build/targets/ws407/mcu_fw.jlink` и копирует hex в
   `build/output/mcu_fw_v<версия>.hex`.
- Версия прошивки берётся из `project(mcu_project_template VERSION ...)`
  (`CMakeLists.txt:22`) — путь к hex запекается в скрипт **на момент
  сборки**. Поэтому после изменения версии или кода обязательна
  пересборка, иначе скрипт может указывать на старый файл.
- Команды скрипта: `speed 4000`, `Device STM32F407VE`, `Con` (подключение),
  `r` (reset), `h` (halt), `loadfile <hex>` (стирание + запись),
  `r`, `h`, `g` (старт), `q` (выход). Скрипт завершается сам — отдельный
  `q` не нужен.
- `JLinkExe`: `/usr/bin/JLinkExe` -> `/opt/SEGGER/JLink_V786/JLinkExe`
  (V7.86).

## Helper-скрипт

В комплекте скила есть обёртка `scripts/flash_run.sh`. Она находит
скрипт `.jlink` в `build/`, запускает `JLinkExe` с таймаутом, сохраняет
лог и проверяет успех прошивки.

    scripts/flash_run.sh -h        # справка

Параметры:
- `-b DIR` — каталог сборки (по умолчанию `build/`);
- `-s SN`  — serial J-Link для `-SelectEmuBySN` (нужен при нескольких
  отладчиках; подключён один: serial 69665469);
- `-o FILE` — сохранить вывод JLinkExe в FILE;
- `-h` — справка.

## Сценарии

### Пересобрать и прошить

    ninja -C build
    .opencode/skills/flash-ws407/scripts/flash_run.sh

Пересборка обязательна: она регенерирует `.jlink` с актуальным путём к
hex и версией.

### Прошить без пересборки (сборка свежая)

    .opencode/skills/flash-ws407/scripts/flash_run.sh -o flash.log

### Выбрать другой каталог сборки

    .opencode/skills/flash-ws407/scripts/flash_run.sh -b /path/to/build

### Несколько J-Link — указать serial

    .opencode/skills/flash-ws407/scripts/flash_run.sh -s 69665469

### Запустить JLinkExe напрямую

    JLinkExe -CommanderScript build/targets/ws407/mcu_fw.jlink

## Анализ вывода

- Успех: строка `O.K.` (например, `Downloading file ... O.K.`), затем
  `g`/`q`. Код возврата скрипта 0.
- Ищи `ERROR:` / `Failed` / `Cannot connect` — проблема подключения или
  файла, см. раздел «Неисправности».
- `Downloading file: ... done.` — hex найден и записан.
- Полезно сверить в логе путь к hex с фактическим
  `build/output/mcu_fw_v<версия>.hex`.

## Неисправности

- `Cannot connect` / `ERROR: Could not connect` — J-Link занят другим
  процессом (JLinkExe, JLinkRTTViewer, rtt-console). Закрой лишние
  процессы и повтори.
- Подключено несколько J-Link — добавь `-s <serial>` (`-SelectEmuBySN`),
  чтобы выбрать нужный.
- `ERROR: Could not find file ...` — путь к hex в `.jlink` устарел или
  hex удалён; пересобери проект (см. «Как работает прошивка»).
- Нет платы на SWD — проверь питание и кабель; при питании через J-Link
  удостоверься, что подключён и USB, и SWD-лента.
- Скрипт завершился по таймауту без `O.K.` — прошивка не подтверждена;
  посмотри лог.

## Ограничения

- Прошивка сбрасывает МК (`r` до и после записи, `g` запускает ядро) —
  не прошивай, если это недопустимо (например, идёт операция ФН).
- `loadfile` стирает и перезаписывает flash целиком.
- `flash_run.sh` не пересобирает проект сам — это делает `ninja -C build`.
