obj-m := fan_bmp180_driver.o

fan_bmp180_driver-y := fan_bmp180_main.o \
                       fan_bmp180_sensor.o \
                       fan_bmp180_device.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
MODULEDIR := $(CURDIR)

.PHONY: all clean

all:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" modules

clean:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" clean
