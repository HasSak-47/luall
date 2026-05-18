#ifndef __debug_h__
#define __debug_h__

void test_handler(const int argc, const char** argv);

typedef int TestReturnT;
typedef TestReturnT(TestFunc)();
typedef TestReturnT (*TestFuncPtr)();

struct TestDesc {
    char* name;
    TestFuncPtr func;
};

#define ASSERT(expr, msg)                                                      \
    if (!(expr)) {                                                             \
        printf("[ERROR @ %s : %d]: %s\n", __FILE__, __LINE__, msg);            \
        return -1;                                                             \
    }

#define TEST(name)                                                             \
    TestFunc name;                                                             \
    __attribute__((section("ly_test"),                                         \
        used)) static const struct TestDesc lydebug_##name = {#name, name};    \
    TestReturnT name()

#endif
