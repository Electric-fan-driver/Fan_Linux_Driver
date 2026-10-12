#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if (( EUID != 0 )); then
    exec sudo bash "$SCRIPT_DIR/unload_all.sh" "$@"
fi

MODULES=(
    fan_servo_driver
    fan_motor_driver
    fan_led_driver
    fan_lcd_driver
    fan_input_driver
    fan_bmp180_driver
)

# 장치 노드 제거는 드라이버의 해제 경로에 맡긴다.
for name in "${MODULES[@]}"; do
    if [[ -d "/sys/module/$name" ]]; then
        rmmod "$name"
        echo "[해제] $name"
    else
        echo "[건너뜀] $name: 적재되지 않음"
    fi
done
