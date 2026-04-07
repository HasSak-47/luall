
#include <ctype.h>
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

#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "ly_string.h"
#include "path.h"

#define LUA_STATE_MT "rewsh.state"
#define LUA_STATE_VARS_MT "rewsh.state.vars"
#define LUA_STATE_VARS_USER_MT "rewsh.state.vars.user"

// TODO: make it behave like a table if there are multiple values
// like in PATH
int index_state_vars_env(lua_State* L) {
    const char* name = lua_tostring(L, -1);

    size_t len           = strlen(name);
    char* name_uppercase = malloc(len + 1);
    for (size_t i = 0; i < len; ++i) {
        name_uppercase[i] = tolower(name[i]);
    }

    const char* env = getenv(name_uppercase);
    free(name_uppercase);
    if (env != NULL) {
        lua_pushstring(L, env);
        return 1;
    }

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
        *path = clone_path(&state.vars.user.home);
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
        *path = clone_path(&state.vars.cwd);
        luaL_getmetatable(L, LUA_PATH_MT);
        lua_setmetatable(L, -2);
    }
    else if (strcmp(name, "host") == 0) {
        char* temp = string_to_cstring(state.vars.host);
        lua_pushstring(L, temp);
        free(temp);
    }
    else {
        return 0;
    }
    return 1;
}

int index_state(lua_State* L) {
    const char* name = lua_tostring(L, -1);
    if (strcmp(name, "running") == 0) {
        lua_pushboolean(L, state.running);
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
    if (strcmp(name, "running") == 0) {
        state.running = lua_toboolean(L, 3);
    }
    else if (strcmp(name, "reload") == 0) {
        state.reload = lua_toboolean(L, 3);
    }
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

    lua_getglobal(L, "rewsh");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        luaL_error(L, "global 'rewsh' does not exist or is not a table");
        return;
    }

    // "temporal" hacky api extensions
    lua_getfield(L, -1, "api");
    lua_pushcfunction(L, lua_set_raw_mode);
    lua_setfield(L, -2, "set_raw_mode");

    lua_pushcfunction(L, lua_unset_raw_mode);
    lua_setfield(L, -2, "unset_raw_mode");

    lua_pushcfunction(L, lua_enter_alternate_screen);
    lua_setfield(L, -2, "enter_alternate_screen");

    lua_pushcfunction(L, lua_leave_alternate_screen);
    lua_setfield(L, -2, "leave_alternate_screen");
    lua_pop(L, 1);

    // setup state table
    lua_createtable(L, 0, 0);
    luaL_getmetatable(L, LUA_STATE_MT);
    lua_setmetatable(L, -2);
    lua_setfield(L, -2, "state");

    return;
}

// struct PluginHandler {
//     struct Plugin* plugin;
//     union {
//         struct {
//             void* handler;
//             SetupFunction setup;
//             DestructFunction destruct;
//         } c;
//
//         struct {
//         } lua;
//     };
// };
//
// DefineVector(VectorPluginHandler, struct PluginHandler);
//
// // Luall.vars
// struct User {
//     char* name;
//     struct Path home;
// };
//
// struct Vars {
//     struct User user;
//     struct Path cwd;
//     char* host;
//     int error;
//     bool debug;
// };
//
// struct Config {
//     struct Path config;
//     struct Path cache;
//     struct Path plugins;
// };
//
// struct ShellState {
//     struct Vars vars;
//     struct Config config;
//     struct VectorPluginHandler plugins;
//     bool running;
//     bool reload;
//     lua_State* L;
// };
//
// extern struct ShellState state;
//
// void init_shell_state();
// void end_shell_state();
//
// void get_current_state();
// void update_current_state();
//
// void set_raw_mode();
// void unset_raw_mode();
