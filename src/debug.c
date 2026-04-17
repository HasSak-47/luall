#include <stdarg.h>
#include <stdio.h>

#include <debug.h>
#include <state.h>
#include <stdlib.h>
#include "utils.h"

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

extern const struct TestDesc __start_ly_test[];
extern const struct TestDesc __stop_ly_test[];

DefineVector(VectorTestDesc, const struct TestDesc*);

void test_handler(const int argc, const char** argv) {
    const struct TestDesc* begin = __start_ly_test;
    const struct TestDesc* end   = __stop_ly_test;

    struct VectorTestDesc failed_tests = {};

    for (const struct TestDesc* current_test = begin; current_test != end;
        current_test++) {

        printf("running test: %s\n", current_test->name);
        int r = current_test->func();
        if (r != 0)
            vector_push(failed_tests, current_test);
    }

    if (failed_tests.len == 0) {
        printf("no failed tests\n");
        return;
    }
    printf("failed tests\n");
    for (size_t i = 0; i < failed_tests.len; ++i) {
        const struct TestDesc* fail = failed_tests.data[i];
        printf("failed test: %s\n", fail->name);
    }
}

TEST(test_handler_ok) {
    return 0;
}
