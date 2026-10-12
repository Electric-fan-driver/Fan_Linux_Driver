obj-m := fan_lcd_driver.o
fan_lcd_driver-y := fan_lcd_main.o fan_lcd_hw.o fan_lcd_device.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
MODULEDIR := $(CURDIR)

.PHONY: all clean

all:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" modules

clean:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" clean
