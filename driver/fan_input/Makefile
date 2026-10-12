# 최종 커널 모듈명 (fan_input.c 소스 파일명과 겹치지 않게 설정)
obj-m := fan_input_core.o

# 모듈을 구성하는 C 소스들의 오브젝트 파일 목록
fan_input_core-objs := fan_input.o \
                       fan_input_wrapper.o \
                       timer_input.o \
                       fan_event_queue.o

# 헤더 경로 지정 (fan_input/include)
ccflags-y += -I$(src)/include