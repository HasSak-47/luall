#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <debug.h>
#include <state.h>

#include <path_api.h>
#include <state_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <term.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "bindgen_log.h"
#include "logs.h"
#include "ly_string.h"
#include "path.h"

#define LUA_STATE_MT "lyra.state"
#define LUA_STATE_VARS_MT "lyra.state.vars"
#define LUA_STATE_VARS_USER_MT "lyra.state.vars.user"
#define LUA_STATE_VARS_ENV_MT "lyra.state.vars.env"

static struct Path* check_path(lua_State* L, int idx) {
    return (struct Path*)luaL_checkudata(L, idx, LUA_PATH_MT);
}

// NOTE: maybe it would be neet to have it behave like a table if there are
// multiple values like in PATH
int index_state_vars_env(lua_State* L) {
    const char* name = lua_tostring(L, -1);
    const char* env  = getenv(name);
    if (env != NULL) {
        lua_pushstring(L, env);
        return 1;
    }

    return 0;
}

int newindex_state_vars_env(lua_State* L) {
    const char* name = luaL_checkstring(L, 2);

    if (lua_isnil(L, 3)) {
        unsetenv(name);
        return 0;
    }

    const char* value = luaL_checkstring(L, 3);
    setenv(name, value, true);
    return 0;
}

int index_state_vars_user(lua_State* L) {
    const char* name = lua_tostring(L, -1);
    if (strcmp(name, "name") == 0) {
        char* temp = string_to_cstring(state.vars.user.name);
        lua_pushstring(L, temp);
    }
    else if (strcmp(name, "home") == 0) {
        struct Path* path =
            (struct Path*)lua_newuserdata(L, sizeof(struct Path));
        *path = path_clone(&state.vars.user.home);
        luaL_getmetatable(L, LUA_PATH_MT);
        lua_setmetatable(L, -2);
    }
    else {
        return 0;
    }
    return 1;
}

int index_state_vars(lua_State* L) {
    const char* name = lua_tostring(L, -1);
    if (strcmp(name, "user") == 0) {
        lua_createtable(L, 1, 1);
        luaL_getmetatable(L, LUA_STATE_VARS_USER_MT);
        lua_setmetatable(L, -2);
    }
    else if (strcmp(name, "cwd") == 0) {
        struct Path* path =
            (struct Path*)lua_newuserdata(L, sizeof(struct Path));
        *path = path_clone(&state.vars.cwd);
        luaL_getmetatable(L, LUA_PATH_MT);
        lua_setmetatable(L, -2);
    }
    else if (strcmp(name, "env") == 0) {
        lua_createtable(L, 0, 0);
        luaL_getmetatable(L, LUA_STATE_VARS_ENV_MT);
        lua_setmetatable(L, -2);
    }
    else if (strcmp(name, "host") == 0) {
        char* temp = string_to_cstring(state.vars.host);
        lua_pushstring(L, temp);
        free(temp);
    }
    else if (strcmp(name, "error") == 0) {
        lua_pushinteger(L, state.vars.error);
    }
    else if (strcmp(name, "debug") == 0) {
        lua_pushboolean(L, state.vars.log_level >= LEVEL_DEBUG);
    }
    else {
        return 0;
    }
    return 1;
}

int index_state(lua_State* L) {
    const char* name = lua_tostring(L, -1);
    if (strcmp(name, "is_running") == 0) {
        lua_pushboolean(L, state.is_running);
    }
    else if (strcmp(name, "reload") == 0) {
        lua_pushboolean(L, state.reload);
    }
    else if (strcmp(name, "vars") == 0) {
        lua_createtable(L, 0, 0);
        luaL_getmetatable(L, LUA_STATE_VARS_MT);
        lua_setmetatable(L, -2);
    }
    else {
        return 0;
    }
    return 1;
}

int newindex_state(lua_State* L) {
    const char* name = lua_tostring(L, 2);
    if (strcmp(name, "is_running") == 0) {
        state.is_running = lua_toboolean(L, 3);
    }
    else if (strcmp(name, "reload") == 0) {
        state.reload = lua_toboolean(L, 3);
    }
    return 0;
}

int newindex_state_vars(lua_State* L) {
    const char* name = lua_tostring(L, -2);
    log_debug("setting to new index: %s", name);
    if (strcmp(name, "error") == 0) {
        state.vars.error = luaL_checkinteger(L, -1);
        log_debug("setting error code to: %d", state.vars.error);
        return 0;
    }
    else if (strcmp(name, "debug") == 0) {
        state.vars.log_level = lua_toboolean(L, -1) ? LEVEL_DEBUG : LEVEL_WARN;
        set_log_level(state.vars.log_level);
        log_warn("set log level to: %lu", state.vars.log_level);
        return 0;
    }
    log_error("failed to set new index: %s", name);
    return 0;
}

int lua_set_raw_mode(lua_State* _) {
    set_raw_mode();
    return 0;
}

int lua_unset_raw_mode(lua_State* _) {
    unset_raw_mode();
    return 0;
}

int lua_enter_alternate_screen(lua_State* _) {
    enter_alternate_screen();
    return 0;
}

int lua_leave_alternate_screen(lua_State* _) {
    leave_alternate_screen();
    return 0;
}

int lua_change_cwd(lua_State* L) {
    log_debug("changing cwd");
    struct Path path     = {};
    bool should_destruct = false;

    if (lua_type(L, 1) == LUA_TSTRING) {
        path = path_parse(lua_tostring(L, 1));
        path_expand(&path, &state.vars.cwd);
        should_destruct = true;
    }
    else {
        struct Path* input = check_path(L, 1);
        path               = path_clone(input);
        path_expand(&path, &state.vars.cwd);
        should_destruct = true;
    }

    char* path_str = path_get_string(path);
    log_debug("setting path to: %s", path_str);
    if (chdir(path_str) != 0) {
        int error = errno;
        if (should_destruct) {
            path_destruct(&path);
        }
        lua_pushnil(L);
        lua_pushstring(L, strerror(error));
        free(path_str);
        return 2;
    }

    path_destruct(&state.vars.cwd);
    state.vars.cwd = path;
    free(path_str);

    lua_pushboolean(L, true);
    return 1;
}

void create_state_vars_env_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_STATE_VARS_ENV_MT)) {
        lua_pushcfunction(L, index_state_vars_env);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, newindex_state_vars_env);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);
}

void create_state_vars_user_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_STATE_VARS_USER_MT)) {
        lua_pushcfunction(L, index_state_vars_user);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);
}

void create_state_vars_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_STATE_VARS_MT)) {
        lua_pushcfunction(L, index_state_vars);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, newindex_state_vars);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);
}

void create_state_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_STATE_MT)) {
        lua_pushcfunction(L, index_state);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, newindex_state);
        lua_setfield(L, -2, "__newindex");
    }
    lua_pop(L, 1);
}

void state_setup_lua_api(lua_State* L) {
    create_state_metatable(L);
    create_state_vars_metatable(L);
    create_state_vars_user_metatable(L);
    create_state_vars_env_metatable(L);

    lua_getglobal(L, "lyra");

    // "temporal" hacky api extensions
    // shit is even more temporal than imagined
    lua_getfield(L, -1, "core");
    lua_getfield(L, -1, "api");
    lua_pushcfunction(L, lua_set_raw_mode);
    lua_setfield(L, -2, "set_raw_mode");

    lua_pushcfunction(L, lua_unset_raw_mode);
    lua_setfield(L, -2, "unset_raw_mode");

    lua_pushcfunction(L, lua_enter_alternate_screen);
    lua_setfield(L, -2, "enter_alternate_screen");

    lua_pushcfunction(L, lua_leave_alternate_screen);
    lua_setfield(L, -2, "leave_alternate_screen");

    lua_pushcfunction(L, lua_change_cwd);
    lua_setfield(L, -2, "cd");
    lua_pop(L, 1);

    // setup state table
    lua_createtable(L, 0, 0);
    luaL_getmetatable(L, LUA_STATE_MT);
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, "state");

    lua_pop(L, 1);

    return;
}
