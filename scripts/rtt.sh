#!/usr/bin/env bash
#
# rtt.sh - обёртка над rtt-console: снятие логов RTT и/или подача
# команды (например, аппаратный сброс платы).
#
# Использование:
#   rtt.sh [-t TARGET] [-c CMD] [-w SECONDS] [-o FILE] [-p] [-h]
set -euo pipefail

export PYTHONUNBUFFERED=1

TARGET="STM32F072CB"
CMD=""
WAIT=10
OUT=""
PWR=0

usage() {
    cat <<EOF
Usage: rtt.sh [-t TARGET] [-c CMD] [-w SECONDS] [-o FILE] [-p] [-h]

  -t TARGET   target chip name (default: STM32F072CB)
  -c CMD      console command to send (e.g. reset); empty = read only
  -w SECONDS  run time in seconds (default: 10)
  -o FILE     write output to FILE (default: stdout)
  -p          power on target via J-Link
  -h          show this help
EOF
}

while getopts "t:c:w:o:ph" opt; do
    case "$opt" in
        t) TARGET="$OPTARG" ;;
        c) CMD="$OPTARG" ;;
        w) WAIT="$OPTARG" ;;
        o) OUT="$OPTARG" ;;
        p) PWR=1 ;;
        h) usage; exit 0 ;;
        *) usage; exit 1 ;;
    esac
done

args=(-t "$TARGET")
if ((PWR)); then
    args+=(-pwr)
fi

run_console() {
    if [[ -n "$OUT" ]]; then
        timeout "$WAIT" rtt-console "${args[@]}" >"$OUT" 2>/dev/null
    else
        timeout "$WAIT" rtt-console "${args[@]}"
    fi
}

if [[ -n "$CMD" ]]; then
    printf '%s\n' "$CMD" | run_console
else
    run_console
fi
