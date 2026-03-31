#ifndef __STRING_H__
#define __STRING_H__

#include <stdbool.h>
#include "./utils.h"

// non null terminated string
DefineVector(String, char);

struct String string_read_n(const char* chrs, const size_t len);
struct String string_from_cstr(const char* cstr);
void string_concat_cstr(struct String* str, const char* cstr);

char* string_to_cstring(const struct String chars);
bool string_cmp_cstring(struct String c, const char* str);
bool string_cmp(const struct String a, const struct String b);

struct String substring(struct String v, const size_t beg, const size_t end);

DefineVector(VectorString, struct String);

#endif
