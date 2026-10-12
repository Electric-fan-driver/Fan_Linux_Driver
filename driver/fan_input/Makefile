obj-m := fan_input_driver.o
fan_input_driver-y := fan_input_wrapper.o timer_input.o fan_event_queue.o

# fan_input_wrapper.c가 fan_input.c를 포함하므로 별도 연결하지 않는다.
ifneq ($(KERNELRELEASE),)
ccflags-y += -I$(src)/include
endif

include $(if $(src),$(src),$(CURDIR))/../module.mk
