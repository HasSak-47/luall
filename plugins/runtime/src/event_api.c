#include <lauxlib.h>
#include <lua.h>

#include <event_api.h>
#include <logs.h>
#include <state.h>

#include <string.h>
#include "ly_string.h"
#include "plugin/definitions.h"

int add_lua_event(lua_State* L) {
    const char* event_string = lua_tostring(L, 1);
    const char* event_owner  = lua_tostring(L, 1);
    struct Event event       = {};
    if (strcmp(event_string, "key_input") == 0) {
        event.kind = EVENT_KEY_INPUT;
    }
    else if (strcmp(event_string, "enter") == 0) {
        event.kind = EVENT_ENTER;
    }
    else if (strcmp(event_string, "exit") == 0) {
        event.kind = EVENT_EXIT;
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    int reference = luaL_ref(L, LUA_REGISTRYINDEX);

    log_trace("binding lua funcion to a hook");
    struct Hook hook = {
        .kind      = HOOK_KIND_LUA,
        .reference = reference,
    };

    on_event_hook(event, string_from_cstr(event_owner), hook);

    return 0;
}

void event_setup_lua_api(lua_State* L) {
    lua_getglobal(L, "lyra");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        luaL_error(L, "global 'lyra' does not exist or is not a table");
        return;
    }

    lua_getfield(L, -1, "core");
    lua_getfield(L, -1, "api");
    lua_pushcfunction(L, add_lua_event);
    lua_setfield(L, -2, "on_event");
    lua_pop(L, 3);
}
