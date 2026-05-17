#ifndef __LOGS_H__
#define __LOGS_H__

#include "bindgen_log.h"

void __suicide_msg(int line, char* file, char* fmt, ...);
void __log_msg(enum Level level, int line, char* file, const char* fmt, ...);

// __VA_ARGS__ contains the fmt string and the parameters
#define log_error(...) __log_msg(LEVEL_ERROR, __LINE__, __FILE__, __VA_ARGS__)
#define log_warn(...) __log_msg(LEVEL_WARN, __LINE__, __FILE__, __VA_ARGS__)
#define log_info(...) __log_msg(LEVEL_INFO, __LINE__, __FILE__, __VA_ARGS__)
#define log_debug(...) __log_msg(LEVEL_DEBUG, __LINE__, __FILE__, __VA_ARGS__)
#define log_trace(...) __log_msg(LEVEL_TRACE, __LINE__, __FILE__, __VA_ARGS__)

#define debug_printf(...) __log_msg(LEVEL_DEBUG, 0, NULL, __VA_ARGS__)

#define unrecoverable_error(...) __suicide_msg(__LINE__, __FILE__, __VA_ARGS__)

#define temporal_suicide_msg(...) __suicide_msg(__LINE__, __FILE__, __VA_ARGS__)

#define temporal_suicide() temporal_suicide_msg("[?]" 1)

void set_to_foreground();

#endif
