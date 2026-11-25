#!/bin/sh
# Привязать драйвер i2c_tiny_usb к vendor-интерфейсу моста.
#
# Драйвер i2c-tiny-usb (mainline) знает только id 0x0403:0xc631 и
# 0x1c40:0x0534, поэтому наш VID/PID добавляется динамически через
# sysfs new_id. После этого драйвер сам подхватывает устройство при
# каждом подключении (до перезагрузки; правило udev повторяет это
# автоматически).
#
# Использование:
#   sudo i2c-tiny-usb-bind.sh 0483 5740
set -eu

VID="${1:?usage: $0 VID PID}"
PID="${2:?usage: $0 VID PID}"

modprobe i2c_tiny_usb || true

DRV=/sys/bus/usb/drivers/i2c_tiny_usb
if [ ! -d "$DRV" ]; then
    echo "driver i2c_tiny_usb is not available" >&2
    exit 1
fi

# EEXIST (уже добавлен) не считается ошибкой
printf '%s %s\n' "$VID" "$PID" > "$DRV/new_id" 2>/dev/null || true

for itf in /sys/bus/usb/drivers/i2c_tiny_usb/*:*; do
    [ -e "$itf" ] || continue
    echo "bound: $(basename "$itf")"
done
