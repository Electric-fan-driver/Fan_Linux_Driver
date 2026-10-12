#!/bin/bash

# 1. 생성된 디바이스 노드 파일 일괄 삭제
sudo rm -f /dev/fan_bmp180 \
           /dev/fan_encoder_speed \
           /dev/fan_encoder_timer \
           /dev/fan_lcd \
           /dev/fan_led \
           /dev/fan_motor

# 2. 커널 모듈 언로드
sudo rmmod fan_motor_driver
sudo rmmod fan_led_driver
sudo rmmod fan_lcd_driver
sudo rmmod fan_input_driver
sudo rmmod fan_bmp180_driver

echo "모든 드라이버 및 노드 정리 완료"