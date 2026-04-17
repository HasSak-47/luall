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
#include "ly_string.h"
#include "plugin/definitions.h"

struct ShellState state = {};

/**
 * sets the state of the shell
 */
void init_shell_variables() {
    state.running   = true;
    char* host_temp = malloc(256);
    gethostname(host_temp, 256);
    state.vars.host = string_from_cstr(host_temp);
    free(host_temp);

    uid_t uid = getuid();
    // no getpwuid_r cuz it is ez and I (hopefully) just need a single thread
    struct passwd* p     = getpwuid(uid);
    state.vars.user.name = string_from_cstr(p->pw_name);
    state.vars.user.home = path_parse(p->pw_dir);

    char buf[256] = {};
    getcwd(buf, 256);
    state.vars.cwd = path_parse(buf);
}

void init_shell_config() {
    state.manager        = new_plugin_manager();
    state.config.plugins = path_parse(PLUGIN_PATH);
    state.config.config  = path_parse(CONFIG_PATH);
    state.config.cache   = path_parse(CACHE_PATH);

    path_expand(&state.config.plugins, &state.vars.cwd);
    path_expand(&state.config.config, &state.vars.cwd);
    path_expand(&state.config.cache, &state.vars.cwd);
}

int c_plugin_setup(lua_State* L) {
    lua_getfield(L, -1, "handler");
    struct PluginHandler* handler = lua_touserdata(L, -1);

    return handler->c.setup(L);
}

int lua_plugin_setup(lua_State* L) {
    lua_getfield(L, 1, "handler");
    struct PluginHandler* handler = lua_touserdata(L, -1);
    lua_pop(L, 1);

    if (!handler || handler->lua.setup_reference == LUA_NOREF) {
        return 0;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, handler->lua.setup_reference);
    lua_pushvalue(L, 1);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        return lua_error(L);
    }

    return 0;
}

static int package_prepend_path(
    lua_State* L, const char* field, const char* entry) {
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return luaL_error(L, "package table is not available");
    }

    lua_getfield(L, -1, field);
    const char* current = lua_tostring(L, -1);
    if (current && strstr(current, entry) != NULL) {
        lua_pop(L, 2);
        return 0;
    }

    lua_pushfstring(L, "%s;%s", entry, current ? current : "");
    lua_setfield(L, -3, field);
    lua_pop(L, 2);
    return 0;
}

static int register_plugin_module_paths(lua_State* L, const char* plugin_path) {
    size_t base_len             = strlen(plugin_path);
    const char* lua_suffix      = "/lua/?.lua";
    const char* lua_init_suffix = "/lua/?/init.lua";
    const char* c_suffix        = "/?.so";

    char* lua_path      = malloc(base_len + strlen(lua_suffix) + 1);
    char* lua_init_path = malloc(base_len + strlen(lua_init_suffix) + 1);
    char* c_path        = malloc(base_len + strlen(c_suffix) + 1);

    snprintf(lua_path, base_len + strlen(lua_suffix) + 1, "%s%s", plugin_path,
        lua_suffix);
    snprintf(lua_init_path, base_len + strlen(lua_init_suffix) + 1, "%s%s",
        plugin_path, lua_init_suffix);
    snprintf(
        c_path, base_len + strlen(c_suffix) + 1, "%s%s", plugin_path, c_suffix);

    int status = package_prepend_path(L, "path", lua_init_path);
    if (status == 0) {
        status = package_prepend_path(L, "path", lua_path);
    }
    if (status == 0) {
        status = package_prepend_path(L, "cpath", c_path);
    }

    free(lua_path);
    free(lua_init_path);
    free(c_path);
    return status;
}

int plugin_load(lua_State* L) {
    debug_printf("loading plugin...\n");
    const char* path = lua_tostring(L, -1);
    struct Plugin* p = add_plugin(state.manager, path);
    if (p == NULL) {
        debug_printf("failed to load plugin: %s\n", path);
        exit(-1);
        return 0;
    }
    enum PluginKind kind = plugin_get_kind(p);
    debug_printf("loading plugin @ %s\n", path);

    switch (kind) {
    case PLUGIN_KIND_C: {
        struct PluginHandler handler = load_c_plugin(L, p);
        if (!handler.c.handler || !handler.c.setup || !handler.c.destruct) {
            return luaL_error(L,
                "failed to load c plugin '%s' (compilation/linking error)",
                path);
        }
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
    case PLUGIN_KIND_BINARY: {
        struct PluginHandler handler = load_binary_plugin(L, p);
        if (!handler.binary.handler || !handler.binary.setup ||
            !handler.binary.destruct) {
            return luaL_error(L,
                "failed to load c plugin '%s' (compilation/linking error)",
                path);
        }
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
    }
    case PLUGIN_KIND_LUA: {
        struct PluginHandler handler = {
            .plugin = p,
            .lua    = {
                       .setup_reference = LUA_NOREF, .destruct_reference = LUA_NOREF}
        };
        char* plugin_path_str = plugin_get_path(p);
        if (register_plugin_module_paths(L, plugin_path_str) != 0) {
            free(plugin_path_str);
            return lua_error(L);
        }
        const char* plugin_init = "init.lua";
        size_t script_path_len =
            strlen(plugin_path_str) + strlen(plugin_init) + 2;
        char* script_path = malloc(script_path_len);
        snprintf(script_path, script_path_len, "%s/%s", plugin_path_str,
            plugin_init);
        if (luaL_loadfile(L, script_path) != LUA_OK) {
            free(plugin_path_str);
            free(script_path);
            return lua_error(L);
        }

        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            free(plugin_path_str);
            free(script_path);
            return lua_error(L);
        }

        if (!lua_istable(L, -1)) {
            free(plugin_path_str);
            free(script_path);
            return luaL_error(L,
                "failed to load lua plugin '%s': entrypoint must return a table",
                path);
        }

        lua_getfield(L, -1, "setup");
        if (lua_isfunction(L, -1)) {
            handler.lua.setup_reference = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else {
            lua_pop(L, 1);
        }

        lua_getfield(L, -1, "destruct");
        if (lua_isfunction(L, -1)) {
            handler.lua.destruct_reference = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else {
            lua_pop(L, 1);
        }

        lua_pop(L, 1);

        vector_push(state.plugins, handler);
        struct PluginHandler* ptr = &state.plugins.data[state.plugins.len - 1];

        lua_createtable(L, 0, 0);
        lua_pushlightuserdata(L, ptr);
        lua_setfield(L, -2, "handler");
        lua_pushcfunction(L, lua_plugin_setup);
        lua_setfield(L, -2, "setup");

        free(plugin_path_str);
        free(script_path);
    } break;
    default:
        return 0;
    }

    return 1;
}

int empty_plugin_setup(lua_State* L) {
    return 0;
}

static void run_event_enter_hook(struct Hook* hook) {
    debug_printf("running enter event\n");
    if (hook->kind == PLUGIN_KIND_C) {
        hook->actor(state.L);
    }
    else {
        lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
        lua_pcall(state.L, 0, 0, 0);
    }
}

static void run_event_exit_hook(struct Hook* hook) {
    debug_printf("running exit event\n");
    if (hook->kind == PLUGIN_KIND_C) {
        hook->actor(state.L);
    }
    else {
        lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
        lua_pcall(state.L, 0, 0, 0);
    }
}

static void run_event_key_input_hook(struct InputKey key, struct Hook* hook) {
    debug_printf("running input event\n");
    if (hook->kind == PLUGIN_KIND_C) {
        push_input_key(state.L, key);
        hook->actor(state.L);
    }
    else {
        lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
        push_input_key(state.L, key);
        lua_pcall(state.L, 1, 0, 0);
    }
}

void trigger_enter_hook() {
    debug_printf("running enter hooks\n");
    for (size_t i = 0; i < state.hooks.len; ++i) {
        if (state.hooks.data[i].event == EVENT_ENTER) {
            run_event_enter_hook(&state.hooks.data[i]);
        }
    }
}

void trigger_exit_hook() {
    debug_printf("running exit hook...\n");
    for (size_t i = 0; i < state.hooks.len; ++i) {
        if (state.hooks.data[i].event == EVENT_EXIT) {
            run_event_exit_hook(&state.hooks.data[i]);
            ;
        }
    }
}

void trigger_input_hook(struct InputKey key) {
    for (size_t i = 0; i < state.hooks.len; ++i) {
        if (state.hooks.data[i].event == EVENT_KEY_INPUT) {
            run_event_key_input_hook(key, &state.hooks.data[i]);
            ;
        }
    }
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
    create_input_key_metatable(state.L);
    lua_newtable(state.L);
    init_plugin_table();
    // NOTE: maybe rename to bootstrap
    // since it is meant to be just used to load core lol
    lua_setfield(state.L, -2, "plugin");
    lua_pushlightuserdata(state.L, &state);
    lua_setfield(state.L, -2, "state");

    lua_setglobal(state.L, "rewsh");

    init_shell_variables();
    init_shell_config();

    char* path = path_get_string(state.config.config);
    debug_printf("running init: %s\n", path);
    if (luaL_dofile(state.L, path) != LUA_OK) {
        const char* err = lua_tostring(state.L, -1); // error message on stack
        printf("Lua error: %s\n", err);
        lua_pop(state.L, 1); // remove error message
        exit(-1);
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
    delete_plugin_manager(state.manager);
    free(state.vars.host.data);
    free(state.vars.user.name.data);

    path_destruct(&state.vars.cwd);
    path_destruct(&state.vars.user.home);
    path_destruct(&state.config.plugins);
    path_destruct(&state.config.config);

    for (size_t i = 0; i < state.plugins.len; ++i) {
        struct PluginHandler* handler = &state.plugins.data[i];
        enum PluginKind kind          = plugin_get_kind(handler->plugin);
        if (kind == PLUGIN_KIND_C) {
            handler->c.destruct(state.L);
        }
        else if (kind == PLUGIN_KIND_LUA &&
                 handler->lua.destruct_reference != LUA_NOREF) {
            lua_rawgeti(
                state.L, LUA_REGISTRYINDEX, handler->lua.destruct_reference);
            if (lua_pcall(state.L, 0, 0, 0) != LUA_OK) {
                printf("Lua plugin destruct error: %s\n",
                    lua_tostring(state.L, -1));
                lua_pop(state.L, 1);
            }
        }
    }

    lua_close(state.L);

    for (size_t i = 0; i < state.plugins.len; ++i) {
        struct PluginHandler* handler = &state.plugins.data[i];
        enum PluginKind kind          = plugin_get_kind(handler->plugin);
        if (kind == PLUGIN_KIND_C) {
            dlclose(handler->c.handler);
        }
    }

    free(state.plugins.data);
    free(state.hooks.data);
    state = (struct ShellState){};
}

void add_hook(enum Event event, Actor actor) {
    struct Hook hook = {
        .kind  = PLUGIN_KIND_C,
        .event = event,
        .actor = actor,
    };

    vector_push(state.hooks, hook);
}
