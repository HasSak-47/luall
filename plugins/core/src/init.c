#include <lauxlib.h>
#include <lualib.h>

#include <plugin/definitions.h>

#include <path_api.h>
#include <process.h>
#include <state_api.h>
#include "event_api.h"
#include "lua.h"

int plugin_setup(lua_State* L) {
    luaL_requiref(L, "base", luaopen_base, true);
    luaL_requiref(L, "io", luaopen_io, true);
    luaL_requiref(L, "math", luaopen_math, true);
    luaL_requiref(L, "table", luaopen_table, true);
    luaL_requiref(L, "package", luaopen_package, true);
    luaL_requiref(L, "string", luaopen_string, true);
    luaL_requiref(L, "os", luaopen_os, true);

    lua_getglobal(L, "rewsh");
    lua_newtable(L);

    lua_newtable(L);
    lua_setfield(L, -2, "api");
    lua_newtable(L);
    lua_setfield(L, -2, "state");

    lua_setfield(L, -2, "core");

    process_setup_lua_api(L);
    path_setup_lua_api(L);
    state_setup_lua_api(L);
    event_setup_lua_api(L);

    return 0;
}

int plugin_destruct(lua_State* _) {
    return 0;
}
