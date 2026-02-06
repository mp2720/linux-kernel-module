.PHONY: clean all

CFLAGS ?= -Wall -Wextra -Werror -std=gnu99 -O2
LDFLAGS ?= -static
SOURCES=main.c
BUILD_DIR=build

all: $(BUILD_DIR)/test_module_ctl

$(BUILD_DIR)/test_module_ctl: $(SOURCES) $(BUILD_DIR)
	gcc $(CFLAGS) $(LDFLAGS) -o "$@" $(SOURCES)

$(BUILD_DIR):
	mkdir -p "$@"

clean:
	rm -rf "$(BUILD_DIR)"

compile_commands.json: clean
	bear -- "$(MAKE)" -C "$(CURDIR)"
