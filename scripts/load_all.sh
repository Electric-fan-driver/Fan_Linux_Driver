#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd -- "$SCRIPT_DIR/.." && pwd)
if (( EUID != 0 )); then
    exec sudo bash "$SCRIPT_DIR/load_all.sh" "$@"
fi

# 입력 모듈은 GPIO IRQ 가 기본으로 꺼져 있어서 파라미터가 필요하다. 환경변수 FAN_INPUT_PARAMS 로 바꿀 수 있다.
# 풍량 엔코더는 기본 핀(7,11,36번), 타이머 엔코더는 12번=A, 35번=B, 38번=누름에 맞춘 값이다.
FAN_INPUT_PARAMS=${FAN_INPUT_PARAMS:-"enable_gpio=1 timer_enable_gpio=1 timer_gpio_key=400 timer_gpio_s2=398 timer_gpio_s1=401"}

MODULE_FILES=(
    driver/fan_bmp180_driver.ko
    driver/fan_input/fan_input_core.ko
    driver/fan_lcd_driver.ko
    driver/fan_led_driver.ko
    driver/fan_motor_driver.ko
    driver/fan_servo_driver.ko
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
        params=""
        if [[ "$name" == "fan_input_core" ]]; then
            params="$FAN_INPUT_PARAMS"
        fi
        insmod "$PROJECT_DIR/$file" $params
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
