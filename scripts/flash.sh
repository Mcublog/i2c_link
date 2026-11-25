#!/usr/bin/env bash
#
# flash.sh - прошивка платы fntester_pcb (STM32F072CB) через J-Link.
# Находит JLink-скрипт, сгенерированный CMake из cmake/burn.jlink.in,
# запускает JLinkExe и проверяет результат.
#
# Использование:
#   flash.sh [-b BUILD_DIR] [-s SN] [-o FILE] [-h]
set -euo pipefail

BUILD_DIR="build"
SN=""
OUT=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
JLINK_SCRIPT="targets/fntester_pcb/fntester_pcb.jlink"

usage() {
    cat <<EOF
Usage: flash.sh [-b BUILD_DIR] [-s SN] [-o FILE] [-h]

  -b DIR   build directory (default: build/)
  -s SN    J-Link serial for -SelectEmuBySN (needed when several
           J-Links are connected; connected one: 69665469)
  -o FILE  write JLinkExe output to FILE (default: stdout)
  -h       show this help
EOF
}

while getopts "b:s:o:h" opt; do
    case "$opt" in
        b) BUILD_DIR="$OPTARG" ;;
        s) SN="$OPTARG" ;;
        o) OUT="$OPTARG" ;;
        h) usage; exit 0 ;;
        *) usage; exit 1 ;;
    esac
done

if [[ "$BUILD_DIR" == /* ]]; then
    build_root="$BUILD_DIR"
else
    build_root="$PROJ_DIR/$BUILD_DIR"
fi
script="$build_root/$JLINK_SCRIPT"
if [[ ! -f "$script" ]]; then
    echo "error: $script not found" >&2
    echo "build first: cmake --build $BUILD_DIR (regenerates .jlink and .hex)" >&2
    exit 1
fi

args=(-CommanderScript "$script" -ExitOnError 1)
if [[ -n "$SN" ]]; then
    args+=(-SelectEmuBySN "$SN")
fi

if [[ -n "$OUT" ]]; then
    set +e
    timeout 60 JLinkExe "${args[@]}" >"$OUT" 2>&1
    rc=$?
    set -e
else
    set +e
    timeout 60 JLinkExe "${args[@]}"
    rc=$?
    set -e
fi

if (( rc != 0 )); then
    echo "error: JLinkExe failed (rc=$rc)" >&2
    [[ -n "$OUT" ]] && echo "log: $OUT" >&2
    exit $rc
fi

if [[ -n "$OUT" ]] && ! grep -q "O.K." "$OUT"; then
    echo "error: no 'O.K.' in JLink output" >&2
    exit 1
fi

echo "flash OK: $script"
