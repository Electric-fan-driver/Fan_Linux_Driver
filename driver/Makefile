obj-m := bmp180_demo.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
MODULEDIR := $(CURDIR)

.PHONY: all clean

all:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" modules

clean:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" clean
