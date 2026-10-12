CC = aarch64-linux-gnu-gcc
CPPFLAGS += -Iinclude -I../include
CFLAGS = -Wall -Wextra -O2 -MMD -MP
LDFLAGS := -pthread
TARGET  := main

# 컴파일할 소스 파일 목록
SRCS    := $(wildcard src/*.c)        

# 오브젝트 파일 및 의존성 파일
OBJS    := $(SRCS:.c=.o)
DEPS    := $(SRCS:.c=.d)

all: $(TARGET)

.PHONY: all clean

bmp180_test: src/bmp180.c tests/bmp180_test.c include/fan_api.h ../include/fan_ioctl.h
	$(CC) $(CPPFLAGS) -Wall -Wextra -O2 src/bmp180.c tests/bmp180_test.c -o $@

lcd_display_test: src/display.c tests/lcd_display_test.c include/fan_api.h ../include/fan_ioctl.h
	$(CC) $(CPPFLAGS) -Wall -Wextra -O2 src/display.c tests/lcd_display_test.c -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) bmp180_test lcd_test lcd_display_test
