#include <logs.h>
#include <state.h>

#include <stdarg.h>
#include <stdio.h>

void __error_msg(int line, char* file, char* fmt, char* msg, int code) {
    printf(fmt, file, line, msg);
    exit(code);
}

static void __debug_printf_set_debug(const char* fmt, va_list args) {
    vprintf(fmt, args);
}

static void __debug_printf_unset_debug(const char* fmt, va_list args) {}

typedef void (*__debug_printf)(const char*, va_list args);

static __debug_printf __functions[] = {
    __debug_printf_unset_debug, __debug_printf_set_debug};

void debug_printf(const char* fmt, ...) {
    va_list(args);
    va_start(args, fmt);
    __functions[state.vars.debug](fmt, args);
    va_end(args);
}
