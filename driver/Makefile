KERNELDIR ?= /lib/modules/$(shell uname -r)/build

ifneq ($(KERNELRELEASE),)
# 각 장치의 Makefile이 모듈 구성 소스를 관리한다.
obj-m := bmp180/ lcd1602/ fan_input/ led/ motor/ servo/
else
.DEFAULT_GOAL := all
.PHONY: all clean
all:
	$(MAKE) -C "$(KERNELDIR)" M="$(CURDIR)" modules
clean:
	$(MAKE) -C "$(KERNELDIR)" M="$(CURDIR)" clean
endif
