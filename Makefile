# --- 타깃 커널 및 경로 설정 ---
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# 크로스 컴파일 필요 시 주석 해제 후 환경에 맞게 사용
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

# --- 1. 유저 애플리케이션 설정 ---
CC ?= gcc
APP_TARGET := fan_app
APP_SRCS := $(wildcard app/src/*.c)
APP_OBJS := $(APP_SRCS:.c=.o)
APP_INCLUDES := -Iapp/include -Iinclude
# pthread 사용 시 -pthread 추가
CFLAGS += $(APP_INCLUDES) -Wall -O2 -pthread

# --- 2. 커널 모듈(드라이버) 설정 ---
# driver 폴더 내 각 C 파일들을 개별 모듈(.ko)로 빌드
# 추가예정
obj-m += driver/fan_bmp180_driver.o
obj-m += driver/fan_input_driver.o
obj-m += driver/fan_lcd_driver.o
obj-m += driver/fan_led_driver.o
obj-m += driver/fan_motor_driver.o

# 드라이버에서 include/fan_ioctl.h 등을 참조할 수 있도록 헤더 경로 전달
ccflags-y := -I$(PWD)/include

# --- 기본 타깃 ---
all: app modules

# 앱 빌드
app: $(APP_TARGET)

$(APP_TARGET): $(APP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

app/src/%.o: app/src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 커널 모듈 전체 빌드
modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# 디바이스 트리 오버레이 빌드 (.dtbo 생성 필요 시)
dtbo: dts/fan-overlay.dts
	dtc -@ -I dts -O dtb -o dts/fan-overlay.dtbo dts/fan-overlay.dts

# 클린업
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(APP_TARGET) $(APP_OBJS) dts/*.dtbo