# --- 타깃 커널 및 경로 설정 ---
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(CURDIR)

# 크로스 컴파일 설정
ARCH          ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

export ARCH CROSS_COMPILE

# ==============================================================================
# 1. Kbuild 영역 (커널 빌드 시스템이 M=$(PWD)로 재진입했을 때 실행)
# ==============================================================================
ifneq ($(KERNELRELEASE),)

    # 1. BMP180 모듈
    obj-m += driver/bmp180/fan_bmp180_driver.o
    driver/bmp180/fan_bmp180_driver-y := fan_bmp180_device.o \
                                         fan_bmp180_main.o \
                                         fan_bmp180_sensor.o

    # 2. LCD1602 모듈
    obj-m += driver/lcd1602/fan_lcd_driver.o
    driver/lcd1602/fan_lcd_driver-y := fan_lcd_device.o \
                                       fan_lcd_hw.o \
                                       fan_lcd_main.o

    # 3. LED 모듈
    obj-m += driver/led/fan_led_driver.o
    driver/led/fan_led_driver-y := fan_led_main.o

    # 4. Motor 모듈
    obj-m += driver/motor/fan_motor_driver.o
    driver/motor/fan_motor_driver-y := fan_motor_main.o

    # 5. Servo 모듈
    obj-m += driver/servo/fan_servo_driver.o
    driver/servo/fan_servo_driver-y := fan_servo_main.o

    # 6. Fan Input (단독 디렉터리)
    obj-m += driver/fan_input/

    # 헤더 인클루드 경로
    ccflags-y := -I$(PWD)/include \
                 -I$(PWD)/driver/fan_input/include \
                 -I$(PWD)/driver/bmp180 \
                 -I$(PWD)/driver/lcd1602

endif

# ==============================================================================
# 2. 일반 Make 영역 (유저가 터미널에서 make를 실행했을 때)
# ==============================================================================
else

    # --- 유저 애플리케이션 빌드 설정 ---
    CC           := $(CROSS_COMPILE)gcc
    APP_TARGET   := app/fan_app
    
    # [수정] test_fsm_mock.c 제외 처리 (필요 시 수정)
    ALL_SRCS     := $(wildcard app/src/*.c)
    APP_SRCS     := $(filter-out app/src/test_fsm_mock.c, $(ALL_SRCS))
    APP_OBJS     := $(APP_SRCS:.c=.o)
    
#     APP_INCLUDES := -Iapp/include -Iinclude
    APP_INCLUDES := -Iapp/include -Iinclude -Idriver/fan_input/include
    CFLAGS       += $(APP_INCLUDES) -Wall -O2 -pthread

    .PHONY: all app modules dtbo clean

    all: app modules

    # 유저 앱 빌드
    app: $(APP_TARGET)

    $(APP_TARGET): $(APP_OBJS)
		$(CC) $(CFLAGS) -o $@ $^

    app/src/%.o: app/src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

    # 커널 모듈 빌드
    modules:
		$(MAKE) -C $(KDIR) M=$(PWD) modules

    # 디바이스 트리 오버레이 빌드 (.dtbo)
    dtbo: dts/fan-overlay.dtbo

    dts/fan-overlay.dtbo: dts/fan-overlay.dts
		dtc -@ -I dts -O dtb -o $@ $<

    # 클린업
    clean:
		$(MAKE) -C $(KDIR) M=$(PWD) clean
		rm -f $(APP_TARGET) $(APP_OBJS) dts/*.dtbo

endif