#include <lauxlib.h>
#include <lualib.h>

#include <plugin/definitions.h>

#include <event_api.h>
#include <log_api.h>
#include <logs.h>
#include <path_api.h>
#include <process.h>
#include <state_api.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include "io_api.h"

int plugin_setup(lua_State* L) {
    log_debug("initing runtime plugin...");
    luaL_requiref(L, "base", luaopen_base, true);
    luaL_requiref(L, "io", luaopen_io, true);
    luaL_requiref(L, "math", luaopen_math, true);
    luaL_requiref(L, "table", luaopen_table, true);
    luaL_requiref(L, "package", luaopen_package, true);
    luaL_requiref(L, "string", luaopen_string, true);
    luaL_requiref(L, "os", luaopen_os, true);

    log_debug("getting lyra");
    lua_getglobal(L, "lyra");
    if (lua_isnil(L, -1)) {
        printf("fuck!\n");
        exit(-1);
    }

    log_debug("creating lyra.api");
    lua_getfield(L, -1, "api");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setfield(L, -2, "api");
    }

    log_debug("creating lyra.core");
    log_debug("creating lyra.core.api");
    lua_getfield(L, -1, "core");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_newtable(L);
        lua_setfield(L, -2, "api");
        lua_setfield(L, -2, "core");
    }

    log_debug("creating lyra.state");
    lua_getfield(L, -1, "state");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setfield(L, -2, "state");
    }

    log_debug("finished expanding lyra namespace");
    process_setup_lua_api(L);
    path_setup_lua_api(L);
    state_setup_lua_api(L);
    log_setup_lua_api(L);
    io_setup_lua_api(L);
    event_setup_lua_api(L);

    return 0;
}

int plugin_destruct(lua_State* _) {
    return 0;
}
