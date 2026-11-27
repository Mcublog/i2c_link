#!/usr/bin/env bash

# Опрос INA219 через мост i2c-tiny-usb (плата fntester_pcb).
#
# Наш адаптер виден ядру как /dev/i2c-N (драйвер i2c-tiny-usb), номер шины
# зависит от порядка подключения, поэтому по умолчанию шина определяется
# автоматически по имени в /sys/class/i2c-dev/i2c-N/name.
#
# INA219 (0x40) сидит на I2C2 (PB10/PB11). Регистры читаются только через
# i2ctransfer: i2cget отдаёт слово в SMBus-порядке (LSB first), а INA219
# выдаёт MSB first, и значение получается байт-перевёрнутым.
#
# Использование:
#   ./scripts/read_ina219.sh                 # один замер
#   ./scripts/read_ina219.sh -i 1            # опрос раз в секунду
#   ./scripts/read_ina219.sh -i 0.2 -n 50    # 50 замеров с шагом 200 мс
#   ./scripts/read_ina219.sh -r 0.01 --raw   # шунт 10 мОм, сырые регистры
#
# Пример вывода:
#   INA219 @ 0x40 на i2c-8: CONFIG = 0x399f
#   time        Vbus, V  Vshunt,mV   I, mA    P, mW  flags
#   12:00:00.123   28.900      -0.040      -0.40     -11.6  ok
#
# Опции:
#   -b, --bus N        номер шины i2c-N (по умолчанию автоопределение)
#   -a, --addr ADDR    адрес INA219 (по умолчанию 0x40)
#   -r, --shunt OHM    сопротивление шунта в Омах (по умолчанию 0.1)
#   -i, --interval SEC период опроса в секундах (0 -- один замер)
#   -n, --count N      число замеров (0 -- бесконечно, нужен --interval)
#       --raw          печатать сырые 16-битные регистры
#   -h, --help         эта справка
#
# Ток считается из напряжения шунта и --shunt (регистр CALIBRATION не нужен),
# мощность -- как Vbus * I. Vbus -- шина после шунта, значение 4 мВ/LSB.
set -euo pipefail

addr=0x40
bus=""
shunt=0.1
interval=0
count=1
count_set=0
raw=0

usage() {
    sed -n '3,34p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        -b|--bus)
            bus="${2:?--bus требует номер шины}"
            shift 2
            ;;
        -a|--addr)
            addr="${2:?--addr требует адрес}"
            shift 2
            ;;
        -r|--shunt)
            shunt="${2:?--shunt требует сопротивление}"
            shift 2
            ;;
        -i|--interval)
            interval="${2:?--interval требует период}"
            shift 2
            ;;
        -n|--count)
            count="${2:?--count требует число замеров}"
            count_set=1
            shift 2
            ;;
        --raw)
            raw=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "неизвестная опция: $1 (см. --help)" >&2
            exit 2
            ;;
    esac
done

command -v i2ctransfer >/dev/null 2>&1 || {
    echo "не найден i2ctransfer (пакет i2c-tools)" >&2
    exit 1
}

if ! awk -v v="$interval" 'BEGIN { exit !(v >= 0) }'; then
    echo "некорректный --interval: $interval" >&2
    exit 2
fi
if ! awk -v v="$shunt" 'BEGIN { exit !(v > 0) }'; then
    echo "некорректный --shunt: $shunt" >&2
    exit 2
fi

# Период без ограничения числа замеров означает бесконечный опрос.
interval_positive() {
    awk -v v="$interval" 'BEGIN { exit !(v > 0) }'
}

if [ "$count_set" -eq 0 ] && interval_positive; then
    count=0
fi
if [ "$count" -eq 0 ] && ! interval_positive; then
    echo "--count 0 требует --interval" >&2
    exit 2
fi

# Найти шину адаптера по имени, которое выставляет драйвер i2c-tiny-usb.
detect_bus() {
    local dev name

    for dev in /sys/class/i2c-dev/i2c-*; do
        [ -r "$dev/name" ] || continue
        name=$(cat "$dev/name")
        case "$name" in
            *i2c-tiny-usb*)
                printf '%s\n' "${dev##*-}"
                return 0
                ;;
        esac
    done
    return 1
}

if [ -z "$bus" ]; then
    if ! bus=$(detect_bus); then
        echo "шина i2c-tiny-usb не найдена: проверьте" \
             "scripts/i2c-tiny-usb-bind.sh и 'i2cdetect -l'" >&2
        exit 1
    fi
fi

if [ ! -e "/dev/i2c-${bus}" ]; then
    echo "/dev/i2c-${bus} не существует" >&2
    exit 1
fi

# Прочитать 16-битный регистр INA219 (MSB first) в переменную reg_value.
# Каждый вызов -- отдельная транзакция write + STOP + read: repeated start
# мост пока не умеет, INA219 такой сценарий поддерживает.
reg_value=0
vsh_raw=0
vbus_raw=0
cal_raw=0
read_reg() {
    local out b0 b1

    out=$(i2ctransfer -y "$bus" "w1@$addr" "$1" r2 2>/dev/null) || return 1
    # shellcheck disable=SC2086
    set -- $out
    [ $# -eq 2 ] || return 1
    b0=$(( $1 ))
    b1=$(( $2 ))
    reg_value=$(( (b0 << 8) | b1 ))
}

# Проверить присутствие INA219 и прочитать CONFIG один раз.
if ! read_reg 0x00; then
    echo "INA219 не отвечает на $addr (шина i2c-${bus});" \
         "проверьте питание и адрес" >&2
    exit 1
fi
config=$reg_value

printf 'INA219 @ %s на i2c-%s: CONFIG = 0x%04x\n' "$addr" "$bus" "$config"
if [ $(( config & 0x8000 )) -ne 0 ]; then
    echo "внимание: у CONFIG взведён бит RESET (0x8000)" >&2
fi

if [ "$raw" -eq 1 ]; then
    printf '%-12s %8s %9s %8s %8s  %-6s %s\n' \
        "time" "Vbus, V" "Vshunt,mV" "I, mA" "P, mW" "flags" \
        "shunt_raw bus_raw cal_raw"
else
    printf '%-12s %8s %9s %8s %8s  %s\n' \
        "time" "Vbus, V" "Vshunt,mV" "I, mA" "P, mW" "flags"
fi

# Сделать один замер: заполнить vsh_raw, vbus_raw, cal_raw и напечатать строку.
sample() {
    local ts flags line

    read_reg 0x01 || return 1
    vsh_raw=$reg_value
    read_reg 0x02 || return 1
    vbus_raw=$reg_value
    read_reg 0x05 || return 1
    cal_raw=$reg_value

    # Бит CNVR (0x02) взводится после каждого преобразования и в опросе
    # всегда равен 1, поэтому тревогой не считается: показываем только OVF.
    flags="ok"
    [ $(( vbus_raw & 0x01 )) -ne 0 ] && flags="OVF"

    line=$(awk -v vsh="$vsh_raw" -v vbus="$vbus_raw" -v r="$shunt" '
        function s16(v) { return (v >= 32768) ? v - 65536 : v }
        BEGIN {
            sv = s16(vsh) * 10 / 1000;      # мВ, 10 мкВ на LSB
            bv = int(vbus / 8) * 4 / 1000;  # В, 4 мВ на LSB, биты 3..15
            i  = sv / r;                    # мА
            p  = bv * i;                    # мВт
            printf "%8.3f %9.3f %8.2f %8.2f", bv, sv, i, p
        }')

    ts=$(date +%H:%M:%S.%3N)
    if [ "$raw" -eq 1 ]; then
        printf '%-12s %s  %-6s 0x%04x 0x%04x 0x%04x\n' \
            "$ts" "$line" "$flags" "$vsh_raw" "$vbus_raw" "$cal_raw"
    else
        printf '%-12s %s  %s\n' "$ts" "$line" "$flags"
    fi
}

done_count=0
while :; do
    if ! sample; then
        echo "ошибка чтения INA219 на $addr (шина i2c-${bus})" >&2
        exit 1
    fi

    done_count=$(( done_count + 1 ))
    if [ "$count" -ne 0 ] && [ "$done_count" -ge "$count" ]; then
        break
    fi
    sleep "$interval"
done
