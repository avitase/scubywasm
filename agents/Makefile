CC            := clang
CLANG_FORMAT  ?= clang-format
CLANG_TIDY    ?= clang-tidy
WASM_TARGET = wasm32-unknown-unknown

WARNINGS := \
	-Werror \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wconversion \
	-Wsign-conversion \
	-Wcast-qual \
	-Wformat=2 \
	-Wundef \
	-Werror=float-equal \
	-Wshadow \
	-Wcast-align \
	-Wunused \
	-Wnull-dereference \
	-Wdouble-promotion \
	-Wimplicit-fallthrough \
	-Werror=strict-prototypes \
	-Wwrite-strings \
	-Wvla

.PHONY: all format lint clean

all: build/simple_agent.wasm 

build:
	mkdir -p build

build/simple_agent.wasm: simple_agent.c | build
	$(CC) --target=$(WASM_TARGET) -std=c23 -nostdlib $(WARNINGS) -O3 -ffunction-sections -fdata-sections \
	-Wl,--no-entry \
	-Wl,--export-memory \
	-Wl,--gc-sections \
	-Wl,--strip-all \
	-Wl,--export=init_agent \
	-Wl,--export=free_context \
	-Wl,--export=set_config_parameter \
	-Wl,--export=clear_world_state \
	-Wl,--export=update_ship \
	-Wl,--export=update_shot \
	-Wl,--export=update_score \
	-Wl,--export=make_action \
	$< -o $@

format:
	$(CLANG_FORMAT) -Wno-error=unknown -i simple_agent.c

check-format:
	$(CLANG_FORMAT) -Wno-error=unknown --dry-run --Werror simple_agent.c

build/lint.stamp: simple_agent.c | build
	$(CLANG_TIDY) simple_agent.c -- -std=c23 -nostdlib $(WARNINGS) -O0
	touch $@

lint: build/lint.stamp

clean:
	rm -rf build/
