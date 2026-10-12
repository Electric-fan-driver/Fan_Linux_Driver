# --- 타깃 커널 및 경로 설정 ---
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(CURDIR)

# 크로스 컴파일 설정
ARCH          ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

export ARCH CROSS_COMPILE

# --- 1. 유저 애플리케이션 빌드 설정 ---
CC           := $(CROSS_COMPILE)gcc
APP_TARGET   := app/fan_app
APP_SRCS     := $(wildcard app/src/*.c)
APP_OBJS     := $(APP_SRCS:.c=.o)
APP_INCLUDES := -Iapp/include -Iinclude
CFLAGS       += $(APP_INCLUDES) -Wall -O2 -pthread

# --- 2. 커널 모듈 (Kbuild) 설정 ---
# driver 폴더 직하위 드라이버들
obj-m += driver/fan_bmp180_driver.o
obj-m += driver/fan_input_driver.o
obj-m += driver/fan_lcd_driver.o
obj-m += driver/fan_led_driver.o
obj-m += driver/fan_motor_driver.o
obj-m += driver/timer.o

# driver/fan_input 폴더로 kbuild 진입
obj-m += driver/fan_input/

# 전체 드라이버가 공통 참조할 헤더 경로
ccflags-y := -I$(PWD)/include

# --- 타깃 규칙 ---
.PHONY: all app modules dtbo clean

all: app modules

# 유저 앱 빌드
app: $(APP_TARGET)

$(APP_TARGET): $(APP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

app/src/%.o: app/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 커널 모듈 전체 빌드
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