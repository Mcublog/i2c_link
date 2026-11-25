#!/bin/sh
# Привязать драйвер i2c-tiny-usb к vendor-интерфейсу моста.
#
# Драйвер i2c-tiny-usb (mainline) знает только id 0x0403:0xc631 и
# 0x1c40:0x0534, поэтому наш VID/PID добавляется динамически через
# sysfs new_id. После этого драйвер сам подхватывает устройство при
# каждом подключении (до перезагрузки; правило udev повторяет это
# автоматически).
#
# Использование:
#   sudo i2c-tiny-usb-bind.sh 1209 21c2
set -eu

VID="${1:?usage: $0 VID PID}"
PID="${2:?usage: $0 VID PID}"

if ! modprobe i2c_tiny_usb; then
    echo "modprobe i2c_tiny_usb failed" >&2
    exit 1
fi

# NOTE: каталог драйвера в sysfs называется по usb_driver.name
# ("i2c-tiny-usb"), а не по имени модуля (i2c_tiny_usb)
DRV=""
for d in /sys/bus/usb/drivers/*i2c*tiny*usb*; do
    if [ -d "$d" ]; then
        DRV="$d"
        break
    fi
done

if [ -z "$DRV" ]; then
    echo "driver i2c-tiny-usb is not available" >&2
    exit 1
fi

# EEXIST (уже добавлен) не считается ошибкой
printf '%s %s\n' "$VID" "$PID" > "$DRV/new_id" 2>/dev/null || true

found=0
for itf in "$DRV"/*:*; do
    [ -e "$itf" ] || continue
    echo "bound: $(basename "$itf")"
    found=1
done

if [ "$found" -eq 0 ]; then
    echo "no interface bound, check 'lsusb -d $VID:$PID'" >&2
    exit 1
fi
