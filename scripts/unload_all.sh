#!/bin/bash

# 1. 생성된 디바이스 노드 파일 일괄 삭제 (와일드카드로 간결화)
sudo rm -f /dev/fan_*

# 2. 커널 모듈 언로드
# 적재된 모듈만 확인하여 언로드 (또는 실패해도 다음 단계 진행하도록 2>/dev/null 처리)
MODULES=(
    fan_motor_driver
    fan_led_driver
    fan_lcd_driver
    fan_input_driver
    fan_bmp180_driver
)

echo "=== 커널 드라이버 모듈 언로드 시작 ==="
for mod in "${MODULES[@]}"; do
    if lsmod | grep -q "^${mod} "; then
        sudo rmmod "$mod" && echo "[-] $mod 언로드 완료"
    else
        echo "[!] $mod: 이미 언로드되었거나 적재되지 않음"
    fi
done

echo "=== 모든 드라이버 및 노드 정리 완료 ==="