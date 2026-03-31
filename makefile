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

SRCS := $(wildcard $(SRC_DIR)/*.c)

OUT_RUST_LIB := $(OBJ_DIR)/librewsh.so 
SRC_RUST_LIB := target/release/librewsh.so

OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o,$(SRCS)) $(OUT_RUST_LIB)

OUT := rewsh

CC := gcc
CFLAGS := -g -fpic -I include -Wall $(LUA_CFLAGS) -DBUNDLE_EXT=\"$(SHLIB_EXT)\"

LDFLAGS := -o $(OUT) -export-dynamic -llua

build: $(OUT)

compileflags:
	@echo -I  > compile_flags.txt
	@echo include/ >> compile_flags.txt
	@echo -Wall >> compile_flags.txt
	@echo $(LUA_CFLAGS) >> compile_flags.txt

rustbuild: $(OUT_RUST_LIB)

$(OUT): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS)

$(OUT_RUST_LIB): Cargo.toml
	cargo build --release
	cp $(SRC_RUST_LIB) $(OUT_RUST_LIB)
	cbindgen -c ./cbindgen.toml --output include/bindgen.h

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: build
	./$(OUT)

test: CFLAGS += -DLY_TEST
test: clean build
	./$(OUT)

clean:
	rm -f $(OBJS)

clean_all: clean

valgrind: shell
	valgrind ./$(OUT)


.PHONY: rustbuild build run test clean cmds source $(OUT_RUST_LIB) compileflags
