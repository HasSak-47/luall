#include <lauxlib.h>
#include <lua.h>

#include <event_api.h>
#include <logs.h>
#include <state.h>

#include <string.h>

int add_lua_event(lua_State* L) {
    const char* event_string = lua_tostring(L, 1);
    enum Event event         = {};
    if (strcmp(event_string, "key_input") == 0) {
        event = EVENT_KEY_INPUT;
    }
    else if (strcmp(event_string, "enter") == 0) {
        event = EVENT_ENTER;
    }
    else if (strcmp(event_string, "exit") == 0) {
        event = EVENT_EXIT;
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    int reference = luaL_ref(L, LUA_REGISTRYINDEX);

    debug_printf("binding lua funcion to a hook\n");
    struct Hook hook = {
        .kind = PLUGIN_KIND_LUA, .event = event, .reference = reference};
    vector_push(state.hooks, hook);

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
