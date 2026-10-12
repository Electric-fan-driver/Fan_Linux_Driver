# 장치별 Makefile에서 공유하는 헤더 경로 및 단독 빌드 규칙.
ifneq ($(KERNELRELEASE),)
ccflags-y += -I$(src)/../../include
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
.DEFAULT_GOAL := all
.PHONY: all clean
all:
	$(MAKE) -C "$(KERNELDIR)" M="$(CURDIR)" modules
clean:
	$(MAKE) -C "$(KERNELDIR)" M="$(CURDIR)" clean
endif
