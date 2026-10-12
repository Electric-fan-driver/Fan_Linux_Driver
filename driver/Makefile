KERNELDIR := /lib/modules/$(shell uname -r)/build
MODULEDIR := $(if $(src),$(src),$(CURDIR))

DRIVERS := $(patsubst %_main.c,%,$(notdir $(wildcard $(MODULEDIR)/*_main.c)))
obj-m := $(addsuffix _driver.o,$(DRIVERS))

$(foreach driver,$(DRIVERS),\
    $(eval $(driver)_driver-y := \
        $(patsubst %.c,%.o,\
            $(filter-out $(driver)_driver.c $(driver)_driver.mod.c,\
                $(notdir $(wildcard $(MODULEDIR)/$(driver)_*.c))))))

.PHONY: all clean

all:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" modules

clean:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" clean
