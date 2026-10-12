CC = aarch64-linux-gnu-gcc
CFLAGS = -c -Iinclude -Wall
LDFLAGS := -pthread

TARGET  := main

# 컴파일할 소스 파일 목록  추후 변경
# SRCS    := src/main.c \
#            src/queue.c \
#            src/core/state_machine.c \
#            src/core/threads.c \
#            src/hal/hal_driver.c

# 오브젝트 파일 및 의존성 파일
OBJS    := $(SRCS:.c=.o)
DEPS    := $(SRCS:.c=.d)

all: $(TARGET)

.PHONY: all clean

bmp180_test: src/bmp180.c tests/bmp180_test.c ../include/fan_api.h ../include/fan_ioctl.h
	$(CC) -I../include -Wall -Wextra -O2 src/bmp180.c tests/bmp180_test.c -o $@

lcd_test: tests/lcd_test.c
	$(CC) -Wall -Wextra -O2 $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) bmp180_test lcd_test
