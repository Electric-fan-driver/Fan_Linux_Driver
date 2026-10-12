#!/bin/bash

# 1. 기존 노드 잔여물 정리
sudo rm -f /dev/fan_*

# 2. 커널 드라이버 모듈 적재
sudo insmod driver/fan_bmp180_driver.ko
sudo insmod driver/fan_input_driver.ko
sudo insmod driver/fan_lcd_driver.ko
sudo insmod driver/fan_led_driver.ko
sudo insmod driver/fan_motor_driver.ko

# 3. /proc/devices에서 등록된 디바이스명 기준으로 메이저 번호 추출
BMP_MAJOR=$(awk '$2=="fan_bmp180_dev" {print $1}' /proc/devices)
INPUT_MAJOR=$(awk '$2=="fan_input_dev" {print $1}' /proc/devices)
LCD_MAJOR=$(awk '$2=="fan_lcd_dev" {print $1}' /proc/devices)
LED_MAJOR=$(awk '$2=="fan_led_dev" {print $1}' /proc/devices)
MOTOR_MAJOR=$(awk '$2=="fan_motor_dev" {print $1}' /proc/devices)

# 4. 디바이스 노드 생성 (엔코더는 동일 메이저 번호에 마이너 번호만 0과 1로 분기)
sudo mknod /dev/fan_bmp180        c $BMP_MAJOR   0
sudo mknod /dev/fan_encoder_speed c $INPUT_MAJOR 0  # 풍량 조절용 (Minor 0)
sudo mknod /dev/fan_encoder_timer c $INPUT_MAJOR 1  # 타이머용 (Minor 1)
sudo mknod /dev/fan_lcd           c $LCD_MAJOR   0
sudo mknod /dev/fan_led           c $LED_MAJOR   0
sudo mknod /dev/fan_motor         c $MOTOR_MAJOR 0

# 5. 유저 프로그램이 접근할 수 있도록 권한 부여
sudo chmod 666 /dev/fan_*

echo "=== 생성된 디바이스 노드 목록 ==="
ls -l /dev/fan_*