KERNELDIR := /lib/modules/$(shell uname -r)/build
MODULEDIR := $(if $(src),$(src),$(CURDIR))

# 공용 헤더는 폴더 깊이에 관계없이 이름으로 include한다.
ccflags-y += -I$(MODULEDIR)/../include

# 루트와 장치별 폴더의 <prefix>_main.c를 자동 탐색한다.
DRIVER_MAIN := $(wildcard $(MODULEDIR)/*_main.c $(MODULEDIR)/*/*_main.c)
obj-m :=

# 기존 <prefix>_driver.ko 이름을 유지하고 소스 경로만 하위 폴더로 지정한다.
define register_driver
obj-m += $(2)_driver.o
$(2)_driver-y := $(patsubst $(MODULEDIR)/%.c,%.o,\
    $(filter-out $(dir $(1))$(2)_driver.c $(dir $(1))$(2)_driver.mod.c,\
        $(wildcard $(dir $(1))$(2)_*.c)))
endef

$(foreach main,$(DRIVER_MAIN),\
    $(eval $(call register_driver,$(main),$(patsubst %_main.c,%,$(notdir $(main))))))

.PHONY: all clean

all:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" modules

clean:
	$(MAKE) -C "$(KERNELDIR)" M="$(MODULEDIR)" clean
