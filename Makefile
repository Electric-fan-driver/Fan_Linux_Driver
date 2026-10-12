# 루트에서는 앱과 드라이버의 빌드를 각 Makefile에 위임한다.
KDIR ?= /lib/modules/$(shell uname -r)/build
DTC ?= dtc
DTS_CPP ?= cpp
DT_BINDINGS_INCLUDE ?= $(KDIR)/include

.DEFAULT_GOAL := all
.PHONY: all app modules dtbo clean
all: app modules
app:
	$(MAKE) -C app
modules:
	$(MAKE) -C driver KERNELDIR="$(KDIR)"

# 원본 DTS의 GPIO 매크로와 include를 매번 전처리한다.
dtbo:
	$(DTS_CPP) -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
		-I"$(DT_BINDINGS_INCLUDE)" dts/fan-overlay.dts \
		-o dts/fan-overlay.preprocessed.dts
	$(DTC) -@ -I dts -O dtb -o dts/fan-overlay.dtbo dts/fan-overlay.preprocessed.dts

clean:
	$(MAKE) -C app clean
	$(MAKE) -C driver KERNELDIR="$(KDIR)" clean
	$(RM) dts/fan-overlay.dtbo
