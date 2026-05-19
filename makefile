SRC_DIR := src

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SHLIB_EXT := dylib
else
SHLIB_EXT := so
endif

RUST_TARGET ?=

LUA_CFLAGS := $(shell pkg-config --cflags lua5.4 2>/dev/null || pkg-config --cflags lua-5.4)
LUA_LIBS := $(shell pkg-config --libs lua5.4 2>/dev/null || pkg-config --libs lua-5.4)

OBJ_DIR := .ignore/build

SRCS := $(wildcard $(SRC_DIR)/*.c) \
        $(wildcard $(SRC_DIR)/**/*.c)

OUT_RUST_LIB := $(OBJ_DIR)/liblyra.so
SRC_RUST_LIB := target/debug/liblyra.so

C_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
OBJS := $(C_OBJS) $(OUT_RUST_LIB)

OBJ_DIRS := $(sort $(dir $(OBJS)))

OUT := lyra

CC := gcc
CFLAGS := -g -fpic -I include -Wall $(LUA_CFLAGS) -DBUNDLE_EXT=\"$(SHLIB_EXT)\"
LDFLAGS := -o $(OUT) -export-dynamic $(LUA_LIBS)

build: $(OBJ_DIRS) $(OUT)

compileflags:
	@echo -I  > compile_flags.txt
	@echo include/ >> compile_flags.txt
	@echo -Wall >> compile_flags.txt
	@echo $(LUA_CFLAGS) >> compile_flags.txt

rustbuild: $(OUT_RUST_LIB)

$(OUT): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS)

$(OUT_RUST_LIB): | $(OBJ_DIRS)
	cargo build
	cp $(SRC_RUST_LIB) $(OUT_RUST_LIB)
	cbindgen -c ./cbindgen_plugin.toml --crate lyra_plugins --output include/bindgen_plugin.h
	cbindgen -c ./cbindgen_log.toml --crate lyra_log --output include/bindgen_log.h
	cbindgen -c ./cbindgen_cli.toml --crate lyra_cli --output include/bindgen_cli.h
	sed -i 's/^Level args_get_level(/enum Level args_get_level(/' include/bindgen_cli.h

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIRS):
	mkdir -p $@

run: build
	./$(OUT)

test: CFLAGS += -DLY_TEST
test: clean build
	./$(OUT)
	cargo test

clean:
	cargo clean
	rm -rf $(OBJ_DIR)
	rm -f $(OUT)

clean_all: clean

valgrind: build
	valgrind ./$(OUT)

.PHONY: rustbuild build run test clean clean_all compileflags
