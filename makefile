SRC_DIR := src

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SHLIB_EXT := dylib
else
SHLIB_EXT := so
endif

RUST_TARGET ?=
CC ?= cc

LUA_CFLAGS := $(shell pkg-config --cflags lua5.4 2>/dev/null || pkg-config --cflags lua-5.4)
LUA_LIBS := $(shell pkg-config --libs lua5.4 2>/dev/null || pkg-config --libs lua-5.4)


OBJ_DIR := .ignore/build

SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/state/*.c)

OUT_RUST_LIB := $(OBJ_DIR)/liblyra.so 
SRC_RUST_LIB := target/debug/liblyra.so

OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS)) $(OUT_RUST_LIB)

OUT := lyra

CC := gcc
CFLAGS := -g -fpic -I include -Wall $(LUA_CFLAGS) -DBUNDLE_EXT=\"$(SHLIB_EXT)\"

LDFLAGS := -o $(OUT) -export-dynamic $(LUA_LIBS)

build: $(OUT)

compileflags:
	@echo -I  > compile_flags.txt
	@echo include/ >> compile_flags.txt
	@echo -Wall >> compile_flags.txt
	@echo $(LUA_CFLAGS) >> compile_flags.txt

rustbuild: $(OUT_RUST_LIB)

$(OUT): $(OUT_RUST_LIB) $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS)

$(OUT_RUST_LIB):
	cargo build
	cp $(SRC_RUST_LIB) $(OUT_RUST_LIB)
	cbindgen -c ./cbindgen_plugin.toml --crate lyra_plugins --output include/bindgen_plugin.h
	cbindgen -c ./cbindgen_log.toml --crate lyra_log --output include/bindgen_log.h
	cbindgen -c ./cbindgen_cli.toml --crate lyra_cli --output include/bindgen_cli.h
	sed -i 's/^Level args_get_level(/enum Level args_get_level(/' include/bindgen_cli.h

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: build
	./$(OUT)

test: CFLAGS += -DLY_TEST
test: clean build
	./$(OUT)
	cargo test

clean:
	cargo clean
	rm -f $(OBJS)

clean_all: clean

valgrind: shell
	valgrind ./$(OUT)


.PHONY: rustbuild build run test clean cmds source $(OUT_RUST_LIB) compileflags
