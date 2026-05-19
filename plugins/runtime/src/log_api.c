#include <ctype.h>
#include <lauxlib.h>
#include <lua.h>

#include <debug.h>
#include <ly_string.h>
#include <path.h>
#include <path_api.h>
#include <stdlib.h>
#include <string.h>
#include "logs.h"

static int _lua_log(lua_State* L, enum Level level) {
    lua_Debug ar;
    // Level 0 = current function
    if (lua_getstack(L, 0, &ar)) {
        // "S" fills source info
        lua_getinfo(L, "S", &ar);

        const char* fmt = lua_tostring(L, -1);
        __log_msg(level, ar.linedefined, ar.source, ar.short_src, fmt);
    }

    return 0;
}

#define MAKE_FUNC(name, level)                                                 \
    int name(lua_State* L) {                                                   \
        return _lua_log(L, level);                                             \
    }

MAKE_FUNC(lua_error, LEVEL_ERROR);
MAKE_FUNC(lua_warn, LEVEL_WARN);
MAKE_FUNC(lua_debug, LEVEL_DEBUG);
MAKE_FUNC(lua_trace, LEVEL_TRACE);

int lua_log(lua_State* L) {
    const char* lvl = lua_tostring(L, -1);
    char* buff      = malloc(strlen(lvl) + 1);
    strcpy(buff, lvl);
    for (size_t i = 0; i < strlen(lvl); ++i) buff[i] = tolower(buff[i]);

    enum Level level = LEVEL_DEBUG;
    if (strcmp(buff, "error")) {
        level = LEVEL_ERROR;
    }
    else if (strcmp(buff, "warn")) {
        level = LEVEL_WARN;
    }
    else if (strcmp(buff, "debug")) {
        level = LEVEL_DEBUG;
    }
    else if (strcmp(buff, "trace")) {
        level = LEVEL_TRACE;
    }

    free(buff);
    lua_Debug ar;

    // Level 0 = current function
    if (lua_getstack(L, 0, &ar)) {
        // "S" fills source info
        lua_getinfo(L, "S", &ar);

        const char* fmt = lua_tostring(L, -1);
        __log_msg(level, ar.linedefined, ar.source, ar.short_src, fmt);
    }

    return 0;
}

void create_log_module(lua_State* L) {
    lua_createtable(L, 0, 0);
    lua_pushcfunction(L, lua_log);
    lua_setfield(L, -2, "log");
    lua_pushcfunction(L, lua_error);
    lua_setfield(L, -2, "error");
    lua_pushcfunction(L, lua_warn);
    lua_setfield(L, -2, "warn");
    lua_pushcfunction(L, lua_debug);
    lua_setfield(L, -2, "debug");
    lua_pushcfunction(L, lua_trace);
    lua_setfield(L, -2, "trace");
}

void log_setup_lua_api(lua_State* L) {
    log_trace("initing logging lua api");
    lua_getglobal(L, "lyra");
    lua_getfield(L, -1, "core");
    lua_getfield(L, -1, "api");

    create_log_module(L);
    lua_setfield(L, -2, "log");

    lua_pop(L, 3);
}
