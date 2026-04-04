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
#include "plugin/definitions.h"

#define LUA_INPUT_KEY_MT "rewsh.input_key"

struct ShellState state = {};

static const char* input_key_kind_name(enum InputKeyKind kind) {
    switch (kind) {
    case INPUT_KEY_KIND_LETTER:
        return "letter";
    case INPUT_KEY_KIND_MODIFIER:
        return "modifier";
    case INPUT_KEY_KIND_SPECIAL:
        return "special";
    case INPUT_KEY_KIND_NONE:
    default:
        return "none";
    }
}

static const char* input_modifier_name(enum InputModifier modifier) {
    switch (modifier) {
    case INPUT_MODIFIER_SHIFT:
        return "shift";
    case INPUT_MODIFIER_ALT:
        return "alt";
    case INPUT_MODIFIER_CTRL:
        return "ctrl";
    case INPUT_MODIFIER_NONE:
    default:
        return NULL;
    }
}

static const char* input_special_name(enum InputSpecialKey special) {
    switch (special) {
    case INPUT_SPECIAL_UP:
        return "up";
    case INPUT_SPECIAL_DOWN:
        return "down";
    case INPUT_SPECIAL_RIGHT:
        return "right";
    case INPUT_SPECIAL_LEFT:
        return "left";
    case INPUT_SPECIAL_ENTER:
        return "enter";
    case INPUT_SPECIAL_TAB:
        return "tab";
    case INPUT_SPECIAL_BACKSPACE:
        return "backspace";
    case INPUT_SPECIAL_ESCAPE:
        return "escape";
    case INPUT_SPECIAL_DELETE:
        return "delete";
    case INPUT_SPECIAL_INSERT:
        return "insert";
    case INPUT_SPECIAL_HOME:
        return "home";
    case INPUT_SPECIAL_END:
        return "end";
    case INPUT_SPECIAL_PAGE_UP:
        return "page_up";
    case INPUT_SPECIAL_PAGE_DOWN:
        return "page_down";
    case INPUT_SPECIAL_F1:
        return "f1";
    case INPUT_SPECIAL_F2:
        return "f2";
    case INPUT_SPECIAL_F3:
        return "f3";
    case INPUT_SPECIAL_F4:
        return "f4";
    case INPUT_SPECIAL_F5:
        return "f5";
    case INPUT_SPECIAL_F6:
        return "f6";
    case INPUT_SPECIAL_F7:
        return "f7";
    case INPUT_SPECIAL_F8:
        return "f8";
    case INPUT_SPECIAL_F9:
        return "f9";
    case INPUT_SPECIAL_F10:
        return "f10";
    case INPUT_SPECIAL_F11:
        return "f11";
    case INPUT_SPECIAL_F12:
        return "f12";
    default:
        return NULL;
    }
}

static void push_nil_or_string(lua_State* L, const char* value) {
    if (value == NULL)
        lua_pushnil(L);
    else
        lua_pushstring(L, value);
}

static int index_input_key(lua_State* L) {
    struct InputKey* key = luaL_checkudata(L, 1, LUA_INPUT_KEY_MT);
    const char* name     = luaL_checkstring(L, 2);

    if (strcmp(name, "kind") == 0) {
        lua_pushstring(L, input_key_kind_name(key->kind));
    }
    else if (strcmp(name, "letter") == 0) {
        if (key->kind == INPUT_KEY_KIND_LETTER)
            lua_pushlstring(L, &key->value.letter, 1);
        else
            lua_pushnil(L);
    }
    else if (strcmp(name, "modifier") == 0) {
        if (key->kind == INPUT_KEY_KIND_MODIFIER)
            push_nil_or_string(L, input_modifier_name(key->value.modifier));
        else
            lua_pushnil(L);
    }
    else if (strcmp(name, "special") == 0) {
        if (key->kind == INPUT_KEY_KIND_SPECIAL)
            push_nil_or_string(L, input_special_name(key->value.special));
        else
            lua_pushnil(L);
    }
    else if (strcmp(name, "modifiers") == 0) {
        lua_pushinteger(L, key->modifiers);
    }
    else if (strcmp(name, "shift") == 0) {
        lua_pushboolean(L, input_key_has_modifier(*key, INPUT_MODIFIER_SHIFT));
    }
    else if (strcmp(name, "alt") == 0) {
        lua_pushboolean(L, input_key_has_modifier(*key, INPUT_MODIFIER_ALT));
    }
    else if (strcmp(name, "ctrl") == 0) {
        lua_pushboolean(L, input_key_has_modifier(*key, INPUT_MODIFIER_CTRL));
    }
    else {
        lua_pushnil(L);
    }

    return 1;
}

static int tostring_input_key(lua_State* L) {
    struct InputKey* key = luaL_checkudata(L, 1, LUA_INPUT_KEY_MT);

    switch (key->kind) {
    case INPUT_KEY_KIND_LETTER:
        lua_pushfstring(L,
            "InputKey{kind=%s, letter=%c, modifiers=%d}",
            input_key_kind_name(key->kind),
            key->value.letter,
            key->modifiers);
        break;
    case INPUT_KEY_KIND_MODIFIER:
        lua_pushfstring(L,
            "InputKey{kind=%s, modifier=%s}",
            input_key_kind_name(key->kind),
            input_modifier_name(key->value.modifier));
        break;
    case INPUT_KEY_KIND_SPECIAL:
        lua_pushfstring(L,
            "InputKey{kind=%s, special=%s, modifiers=%d}",
            input_key_kind_name(key->kind),
            input_special_name(key->value.special),
            key->modifiers);
        break;
    case INPUT_KEY_KIND_NONE:
    default:
        lua_pushstring(L, "InputKey{kind=none}");
        break;
    }

    return 1;
}

static void create_input_key_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_INPUT_KEY_MT)) {
        lua_pushcfunction(L, index_input_key);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, tostring_input_key);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
}

static void push_input_key(lua_State* L, struct InputKey key) {
    struct InputKey* value = lua_newuserdata(L, sizeof(struct InputKey));
    *value                 = key;
    luaL_getmetatable(L, LUA_INPUT_KEY_MT);
    lua_setmetatable(L, -2);
}

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

static void run_event_enter_hook(struct Hook* hook) {
    if (hook->kind == PLUGIN_KIND_C) {
        hook->actor(state.L);
    }
}

static void run_event_exit_hook(struct Hook* hook) {
    if (hook->kind == PLUGIN_KIND_C) {
        hook->actor(state.L);
    }
}

static void run_event_key_input_hook(struct InputKey key, struct Hook* hook) {
    push_input_key(state.L, key);
    if (hook->kind == PLUGIN_KIND_C) {
        hook->actor(state.L);
    }
    else {
        lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
        lua_insert(state.L, -2);
        lua_pcall(state.L, 1, 0, 0);
    }
}

void trigger_enter_hook() {
    debug_printf("running enter hooks\n");
    for (size_t i = 0; i < state.hooks.len; ++i) {
        if (state.hooks.data[i].event == EVENT_KEY_INPUT) {
            run_event_enter_hook(&state.hooks.data[i]);
        }
    }
}

void trigger_exit_hook() {
    debug_printf("running exit hook...\n");
    for (size_t i = 0; i < state.hooks.len; ++i) {
        if (state.hooks.data[i].event == EVENT_KEY_INPUT) {
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
    free(state.vars.host);

    free(state.vars.user.name);

    destruct_path(&state.vars.cwd);
    destruct_path(&state.vars.user.home);
    destruct_path(&state.config.plugins);
    destruct_path(&state.config.config);

    lua_close(state.L);
}

void add_hook(enum Event event, Actor actor) {
    struct Hook hook = {
        .kind  = PLUGIN_KIND_C,
        .event = event,
        .actor = actor,
    };

    vector_push(state.hooks, hook);
}
