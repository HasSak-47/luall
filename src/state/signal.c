#include <lauxlib.h>
#include <lua.h>

#include <state.h>

#include <string.h>

#define LUA_SIGNAL_METATABLE "lyra.signal"

static const char* signal_name(enum Signal signal) {
    switch (signal) {
    case SIGNAL_INT:
        return "int";
    case SIGNAL_TERM:
        return "term";
    case SIGNAL_CONT:
        return "cont";
    case SIGNAL_QUIT:
        return "quit";
    case SIGNAL_HUP:
        return "hup";
    case SIGNAL_CHLD:
        return "chld";
    case SIGNAL_ALRM:
        return "alrm";
    case SIGNAL_PIPE:
        return "pipe";
    case SIGNAL_SEGV:
        return "segv";
    case SIGNAL_FPE:
        return "fpe";
    case SIGNAL_ABRT:
        return "abrt";
    case SIGNAL_USR1:
        return "usr1";
    case SIGNAL_USR2:
        return "usr2";
    case SIGNAL_WINDOW_CHANGE:
        return "window_change";
    default:
        return "unknown";
    }
}

static int index_signal(lua_State* L) {
    enum Signal* signal = luaL_checkudata(L, 1, LUA_SIGNAL_METATABLE);
    const char* name    = luaL_checkstring(L, 2);

    if (strcmp(name, "code") == 0) {
        lua_pushinteger(L, *signal);
    }
    else if (strcmp(name, "name") == 0) {
        lua_pushstring(L, signal_name(*signal));
    }
    else {
        return 0;
    }

    return 1;
}

void create_signal_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_SIGNAL_METATABLE)) {
        lua_pushcfunction(L, index_signal);
        lua_setfield(L, -2, "__index");
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
