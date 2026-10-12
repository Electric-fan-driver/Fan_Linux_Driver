#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
if (( EUID != 0 )); then
    exec sudo bash "$SCRIPT_DIR/load_all.sh" "$@"
fi

MODULE_FILES=(
    driver/bmp180/fan_bmp180_driver.ko
    driver/fan_input/fan_input_driver.ko
    driver/lcd1602/fan_lcd_driver.ko
    driver/led/fan_led_driver.ko
    driver/motor/fan_motor_driver.ko
    driver/servo/fan_servo_driver.ko
)

# 적재 전에 모든 빌드 결과를 확인한다.
for file in "${MODULE_FILES[@]}"; do
    if [[ ! -f "$PROJECT_DIR/$file" ]]; then
        echo "[오류] 모듈 없음: $PROJECT_DIR/$file (make -C driver 실행 필요)" >&2
        exit 1
    fi
done

for file in "${MODULE_FILES[@]}"; do
    name=${file##*/}
    name=${name%.ko}
    if [[ -d "/sys/module/$name" ]]; then
        echo "[유지] $name: 이미 적재됨"
    else
        insmod "$PROJECT_DIR/$file"
        echo "[적재] $name"
    fi
done

# 장치 노드는 드라이버에서 자동 생성한다.
# 입력 GPIO IRQ는 기본 비활성화이며 모듈 파라미터로 별도 설정한다.
if command -v udevadm >/dev/null 2>&1; then
    udevadm settle
fi

NODES=(bmp180 fan_input lcd1602 fan_led fan_motor fan_servo)
missing=0
for name in "${NODES[@]}"; do
    if [[ -c "/dev/$name" ]]; then
        ls -l "/dev/$name"
    else
        echo "[오류] /dev/$name 없음: DT 매칭 및 probe 로그 확인 필요 (sudo dmesg | tail -n 80)." >&2
        missing=1
    fi
done
exit "$missing"
