#include <logs.h>
#include <state.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "bindgen_log.h"

static void __log_msg_varags(enum Level level, int line, const char* file,
    const char* target, const char* fmt, va_list arg) {
    char* buf = NULL;
    vasprintf(&buf, fmt, arg);

    rust_log(level, line, target, file, buf);

    free(buf);
}

void __log_msg(enum Level level, int line, const char* file, const char* target,
    const char* fmt, ...) {
    va_list(args);
    va_start(args, fmt);
    __log_msg_varags(level, line, file, target, fmt, args);
    va_end(args);
}

void __suicide_msg(
    int line, const char* file, const char* target, const char* fmt, ...) {
    va_list(args);
    va_start(args, fmt);
    __log_msg_varags(LEVEL_ERROR, line, file, target, fmt, args);
    va_end(args);
    exit(-1);
}
