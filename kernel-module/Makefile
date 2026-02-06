.PHONY: all clean load unload

PWD=$(CURDIR)
BUILD_DIR ?= $(PWD)/build
KDIR ?= /lib/modules/$(shell uname -r)/build

SOURCES=test_module.c
# MO= есть только в kbuild для Linux >= 6.13
# Так что в build придётся копировать исходные файлы
COPIED_SOURCES=$(SOURCES:%=$(BUILD_DIR)/%)
ifeq (DEBUG, 1)
	CFLAGS += -DDEBUG -g3
endif

all: $(BUILD_DIR)/Makefile
	$(MAKE) -C "$(KDIR)" M="$(BUILD_DIR)" modules

$(BUILD_DIR)/Makefile: $(COPIED_SOURCES)
	echo obj-m += test_module.o > $@
	echo test-module-y += $(COPIED_SOURCES) >> $@
	echo ccflags-y += "$(CFLAGS)" >> $@

$(BUILD_DIR)/%: %
	cp -f "$<" "$@"

$(BUILD_DIR):
	mkdir -p "$@"

compile_commands.json: clean
	bear -- $(MAKE)

clean: $(BUILD_DIR)
	$(MAKE) -C "$(KDIR)" M="$(BUILD_DIR)" clean
	rm -rf "$(BUILD_DIR)"/*

load: $(BUILD_DIR)/test_module.ko
	insmod "$(BUILD_DIR)/test_module.ko"

unload:
	rmmod test_module
