#include <lauxlib.h>
#include <lua.h>

#include <bindgen.h>
#include <debug.h>
#include <path.h>
#include <plugin/loaders.h>
#include <state.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>
#include "lualib.h"
#include "plugin/definitions.h"

struct ShellState state = {};

/**
 * sets the state of the shell
 */
void init_shell_variables() {
    state.running   = true;
    state.vars.host = malloc(256);
    gethostname(state.vars.host, 256);

    uid_t uid = getuid();
    // no getpwuid_r cuz it is ez and I (hopefully) just need a single thread
    struct passwd* p     = getpwuid(uid);
    state.vars.user.name = strdup(p->pw_name);
    state.vars.user.home = parse_path(p->pw_dir);

    const char* _cwd = getenv("PWD");
    state.vars.cwd   = parse_path(_cwd);
}

void init_shell_config() {
    state.config.plugins = parse_path(PLUGIN_PATH);
    state.config.config  = parse_path(CONFIG_PATH);
    state.config.cache   = parse_path(CACHE_PATH);

    expand_path(&state.config.plugins, &state.vars.cwd);
    expand_path(&state.config.config, &state.vars.cwd);
    expand_path(&state.config.cache, &state.vars.cwd);
}

int c_plugin_setup(lua_State* L) {
    lua_getfield(L, -1, "handler");
    struct PluginHandler* handler = lua_touserdata(L, -1);

    return handler->c.setup(L);
}

int plugin_load(lua_State* L) {
    debug_printf("loading plugin...\n");
    const char* path     = lua_tostring(L, -1);
    struct Plugin* p     = get_plugin(path);
    enum PluginKind kind = plugin_get_kind(p);
    debug_printf("loading plugin @ %s\n", path);

    switch (kind) {
    case PLUGIN_KIND_C: {
        struct PluginHandler handler = load_c_plugin(L, p);
        debug_printf("loading c plugin into the internal state\n", path);
        vector_push(state.plugins, handler);
        struct PluginHandler* ptr = &state.plugins.data[state.plugins.len - 1];

        debug_printf(
            "building lua plugin handler with lightuserdata %p\n", ptr);
        lua_createtable(L, 0, 0);
        lua_pushlightuserdata(L, ptr);
        lua_setfield(L, -2, "handler");
        lua_pushcfunction(L, c_plugin_setup);
        lua_setfield(L, -2, "setup");
    } break;
    case PLUGIN_KIND_LUA:
        // TODO: lua plugins
    default:
        return 0;
    }

    return 1;
}

int empty_plugin_setup(lua_State* L) {
    return 0;
}

void init_plugin_table() {
    lua_newtable(state.L);

    /* plugins.load = l_plugins_load */
    lua_pushcfunction(state.L, plugin_load);
    lua_setfield(state.L, -2, "load");

    /* plugins.setup = l_plugin_setup (method available via metatable) */
    lua_pushcfunction(state.L, empty_plugin_setup);
    lua_setfield(state.L, -2, "setup");

    /* plugins.__index = plugins (for method lookup) */
    lua_pushvalue(state.L, -1);
    lua_setfield(state.L, -2, "__index");
}

/**
 * sets the state of the shell
 */
void init_shell_state() {
    state.L = luaL_newstate();
    // luaL_openlibs(state.L);
    lua_newtable(state.L);
    init_plugin_table();
    lua_setfield(state.L, -2, "plugin");
    lua_pushlightuserdata(state.L, &state);
    lua_setfield(state.L, -2, "state");

    lua_setglobal(state.L, "rewsh");

    init_shell_variables();
    init_shell_config();

    char* path = get_path_string(state.config.config);
    debug_printf("running init: %s\n", path);
    if (luaL_dofile(state.L, path) != LUA_OK) {
        const char* err = lua_tostring(state.L, -1); // error message on stack
        printf("Lua error: %s\n", err);
        lua_pop(state.L, 1); // remove error message
    }
    free(path);
}

/**
 * updates state where it is needed
 */
void get_current_state() {}

/**
 * cleanins the shell state
 */
void end_shell_state() {
    free(state.vars.host);

    free(state.vars.user.name);

    destruct_path(&state.vars.cwd);
    destruct_path(&state.vars.user.home);
    destruct_path(&state.config.plugins);
    destruct_path(&state.config.config);

    lua_close(state.L);
}
