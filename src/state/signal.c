#include <lauxlib.h>
#include <lua.h>

#include <state.h>
#define LUA_SIGNAL_METATABLE "lyra.signal"

void create_signal_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_SIGNAL_METATABLE)) {
    }
    lua_pop(L, 1);
}

int push_signal(lua_State* L, enum Signal* signal) {
    enum Signal* s = lua_newuserdata(L, sizeof(enum Signal));
    *s             = *signal;
    luaL_getmetatable(L, LUA_SIGNAL_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}
