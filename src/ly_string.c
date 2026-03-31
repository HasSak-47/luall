#include "ly_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "utils.h"

struct String string_read_n(const char* chrs, const size_t len) {
    struct String cs = {};

    vector_reserve(cs, len);
    for (size_t i = 0; i < len; ++i) {
        vector_push(cs, chrs[i]);
    }

    return cs;
}

char* string_to_cstring(const struct String chars) {
    char* s = malloc((chars.len + 1) * sizeof(char));
    for (size_t i = 0; i < chars.len; ++i) s[i] = chars.data[i];
    s[chars.len] = 0;

    return s;
}

TEST(test_string_to_cstring) {
    const char* test_string = "test string";
    struct String str       = string_from_cstr(test_string);
    char* other             = string_to_cstring(str);

    int ok = strcmp(other, test_string) != 0;
    if (ok) {
        printf("strings: '%s' '%s'\n", other, test_string);
    }
    free(other);
    free(str.data);

    return ok;
}

struct String substring(struct String v, const size_t beg, const size_t end) {
    struct String name = {};
    for (size_t i = beg; i < end; ++i) {
        vector_push(name, v.data[i]);
    }

    return name;
}

bool string_cmp(const struct String a, const struct String b) {
    size_t i = 0;
    while (i < a.len && i < b.len) {
        if (a.data[i] != b.data[i])
            return false;
        ++i;
    }

    return a.len == b.len;
}

bool string_cmp_cstring(struct String c, const char* str) {
    size_t len = strlen(str);
    size_t i   = 0;
    while (i < len && i < c.len) {
        if (str[i] != c.data[i])
            return false;
        ++i;
    }

    return len == c.len;
}

struct String string_from_cstr(const char* cstr) {
    size_t len = strlen(cstr);

    return string_read_n(cstr, len);
}

void string_concat_cstr(struct String* str, const char* cstr) {
    size_t len = strlen(cstr);
    vector_reserve(*str, str->len + len);

    for (size_t i = 0; i < len; ++i) {
        vector_push((*str), cstr[i]);
    }
}
