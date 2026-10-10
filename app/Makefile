CC = aarch64-linux-gnu-gcc
CFLAGS = -Iinclude -Wall -MMD -MP
LDFLAGS := -pthread

TARGET  := main

# 컴파일할 소스 파일 목록
SRCS    := $(wildcard src/*.c)        

# 오브젝트 파일 및 의존성 파일
OBJS    := $(SRCS:.c=.o)
DEPS    := $(SRCS:.c=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
    $(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

clean:
    rm -f $(OBJS) $(DEPS) $(TARGET)