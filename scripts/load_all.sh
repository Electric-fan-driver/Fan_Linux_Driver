#!/bin/bash
set -e

# 1. 기존 노드 잔여물 정리
sudo rm -f /dev/fan_*

# 2. 커널 드라이버 모듈 적재
# 빌드 구조에 맞춘 실제 .ko 파일 경로 지정
sudo insmod driver/bmp180/fan_bmp180_driver.ko
sudo insmod driver/fan_input/fan_input_driver.ko  # 또는 driver/fan_input_driver.ko (빌드 결과 위치에 맞춰 조정)
sudo insmod driver/lcd1602/fan_lcd_driver.ko
sudo insmod driver/fan_led_driver.ko
sudo insmod driver/fan_motor_driver.ko

# 3. /proc/devices에서 등록된 디바이스명 기준으로 메이저 번호 추출
BMP_MAJOR=$(awk '$2=="fan_bmp180_dev" {print $1}' /proc/devices)
INPUT_MAJOR=$(awk '$2=="fan_input_dev" {print $1}' /proc/devices)
LCD_MAJOR=$(awk '$2=="fan_lcd_dev" {print $1}' /proc/devices)
LED_MAJOR=$(awk '$2=="fan_led_dev" {print $1}' /proc/devices)
MOTOR_MAJOR=$(awk '$2=="fan_motor_dev" {print $1}' /proc/devices)

# 메이저 번호 추출 확인 (모듈 등록 실패 시 빠른 중단)
for var in BMP_MAJOR INPUT_MAJOR LCD_MAJOR LED_MAJOR MOTOR_MAJOR; do
    if [ -z "${!var}" ]; then
        echo "[오류] $var 메이저 번호를 /proc/devices에서 찾을 수 없습니다." >&2
        exit 1
    fi
done

# 4. 디바이스 노드 생성 (엔코더는 동일 메이저 번호에 마이너 번호 0, 1 분기)
sudo mknod /dev/fan_bmp180         c $BMP_MAJOR   0
sudo mknod /dev/fan_encoder_speed c $INPUT_MAJOR 0  # 풍량 조절용 (Minor 0)
sudo mknod /dev/fan_encoder_timer c $INPUT_MAJOR 1  # 타이머용 (Minor 1)
sudo mknod /dev/fan_lcd           c $LCD_MAJOR   0
sudo mknod /dev/fan_led           c $LED_MAJOR   0
sudo mknod /dev/fan_motor         c $MOTOR_MAJOR 0

# 5. 유저 프로그램 접근 권한 부여
sudo chmod 666 /dev/fan_*

echo "=== 생성된 디바이스 노드 목록 ==="
ls -l /dev/fan_*