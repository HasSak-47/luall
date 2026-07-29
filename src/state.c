#include <bits/types/siginfo_t.h>
#include <lauxlib.h>
#include <lua.h>

#include <bindgen_plugin.h>
#include <logs.h>
#include <ly_string.h>
#include <path.h>
#include <plugin.h>
#include <plugin/definitions.h>
#include <signal.h>
#include <state.h>

#include <stdio.h>
#include <stdlib.h>

#include <dlfcn.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vectors.h>

struct ShellState state = {};

static void signal_handler(int signal, siginfo_t* info, void* context) {
    enum Signal s = signal;
    log_debug("got signal: %d", signal);
    switch (s) {
    case SIGWINCH: {
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &state.vars.term.window_size);
    } break;
    default:
        break;
    }
    trigger_event(
        (struct Event){
            EVENT_SIGNAL,
            {},
        },
        (PushEventArg)push_signal, &s);
}

/**
 * sets the state of the shell
 */
void init_shell_variables() {
    state.is_running = true;
    char* host_temp  = malloc(256);
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

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &state.vars.term.window_size);

    state.sa.sa_sigaction = signal_handler;
    sigemptyset(&state.sa.sa_mask);

    if (sigaction(SIGWINCH, &state.sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void init_shell_event_handler() {
    struct HookData hd_enter = ((struct HookData){
        .event = (struct Event){.kind = EVENT_ENTER, .name = {}},
        .owner = {},
        .hooks = {},
    });
    struct HookData hd_exit  = ((struct HookData){
        .event = (struct Event){.kind = EVENT_EXIT, .name = {}},
        .owner = {},
        .hooks = {},
    });

    struct HookData hd_input = ((struct HookData){
        .event = (struct Event){.kind = EVENT_KEY_INPUT, .name = {}},
        .owner = {},
        .hooks = {},
    });
    vector_push(state.event.hooks, hd_enter);
    vector_push(state.event.hooks, hd_exit);
    vector_push(state.event.hooks, hd_input);
}

void init_shell_config(const char* config_path, const char* cache_path) {
    state.manager = new_plugin_manager();
    state.config.config =
        path_parse(config_path != NULL ? config_path : CONFIG_PATH);
    state.config.cache =
        path_parse(cache_path != NULL ? cache_path : CACHE_PATH);

    path_expand(&state.config.config, &state.vars.cwd);
    path_expand(&state.config.cache, &state.vars.cwd);
}

int empty_plugin_setup(lua_State* L) {
    return 0;
}

static int lua_plugin_api_config_newindex(lua_State* L) {
    const char* path          = lua_tostring(L, 1);
    struct PluginData* plugin = manager_resolve_plugin(state.manager, path);
    struct PluginHandler* plugin_handler = get_plugin_handler(plugin);
    if (!lua_istable(L, 2)) {
        log_warn("passed a non table config to %s", path);
        return 0;
    }

    if (plugin_handler->setup == 0) {
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, plugin_handler->setup_table_reference);

    return 1;
}

static int lua_plugin_api_resolve(lua_State* L) {
    const char* path        = lua_tostring(L, 1);
    struct PluginData* data = manager_resolve_plugin(state.manager, path);
    struct PluginHandler* plugin_handler = get_plugin_handler(data);
    lua_createtable(L, 0, 0);
    plugin_handler->setup_table_reference = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static int lua_plugin_api_prepare(lua_State* L) {
    const char* path = lua_tostring(L, 1);
    manager_prepare_plugin(state.manager, path);
    return 0;
}

static int lua_plugin_api_require(lua_State* L) {
    const char* path = lua_tostring(L, 1);
    if (!manager_is_plugin_prepared(state.manager, path)) {
        log_debug("preparing plugin %s", path);
        if (manager_prepare_plugin(state.manager, path) == -1) {
            log_debug("failed to prepare plugin %s...", path);
            return 0;
        }
    }

    struct PluginData* d    = get_plugin(state.manager, path);
    struct PluginHandler* h = get_plugin_handler(d);

    log_debug("loading plugin %s", path);
    switch (plugin_get_kind(d)) {
    case PLUGIN_KIND_C:
    case PLUGIN_KIND_RUST:
    case PLUGIN_KIND_BINARY: {
        lua_pushcfunction(L, h->setup);
        lua_rawgeti(L, LUA_REGISTRYINDEX, h->setup_table_reference);
        int code = lua_pcall(L, 1, 1, 0);
        if (code != LUA_OK) {
            log_error("failed to run the plugin %s", path);
            break;
        }

        log_debug("loaded '%s' binary data with code %d", path, code);
        break;
    }
    case PLUGIN_KIND_LUA: {
        if (register_plugin_module_paths(L, h->lua_path) != 0) {
            return lua_error(L);
        }

        char* entrypoint = NULL;
        asprintf(&entrypoint, "%s/init.lua", h->lua_path);
        // loads init file into a func
        int code = luaL_loadfile(L, entrypoint);
        free(entrypoint);
        if (code != LUA_OK) {
            return lua_error(L);
        }

        // runs init file
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            return lua_error(L);
        }

        // must return a table
        if (!lua_istable(L, -1)) {
            return luaL_error(L,
                "failed to require lua plugin '%s': entrypoint must return a table",
                path);
        }

        // get setup and unload fields
        lua_getfield(L, -1, "unload");
        if (lua_isfunction(L, -1)) {
            h->destruct_reference = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else {
            lua_pop(L, 1);
        }

        // run setup
        lua_getfield(L, -1, "setup");
        if (lua_isfunction(L, -1)) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, h->setup_table_reference);

            if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                return lua_error(L);
            }
        }
        else {
            lua_pop(L, 1);
        }
        // no error if setup isn't found it just means that the plugin ran
        // everything already and doesn't export or extend anything and doesn't
        // need a destructor
    }
    }
    log_debug("registering exports and extends");
    // setup must return nil or a table
    if (!lua_isnil(L, -1) && !lua_istable(L, -1)) {
        return luaL_error(L,
            "failed to require lua plugin '%s': setup must return a table "
            "or nil",
            path);
    }
    if (lua_isnil(L, -1)) {
        return 0;
    }
    lua_getfield(L, -1, "exports");
    h->export_reference = luaL_ref(L, -1);
    lua_getfield(L, -1, "extends");
    h->extend_reference = luaL_ref(L, -1);
    lua_pop(L, 1);

    return 0;
}

static int lua_plugin_api_destroy(lua_State* L) {
    return 0;
}

static void init_plugin_table() {
    lua_newtable(state.L);

    lua_pushcfunction(state.L, lua_plugin_api_resolve);
    lua_setfield(state.L, -2, "resolve");

    lua_pushcfunction(state.L, lua_plugin_api_prepare);
    lua_setfield(state.L, -2, "prepare");

    lua_pushcfunction(state.L, lua_plugin_api_require);
    lua_setfield(state.L, -2, "require");

    lua_pushcfunction(state.L, lua_plugin_api_destroy);
    lua_setfield(state.L, -2, "destroy");

    log_debug("initing config handling");
    lua_newtable(state.L);
    lua_pushcfunction(state.L, lua_plugin_api_config_newindex);
    lua_setfield(state.L, -2, "__newindex");
    lua_pushcfunction(state.L, lua_plugin_api_config_newindex);
    lua_setfield(state.L, -2, "__index");

    lua_setfield(state.L, -2, "config");
}

/**
 * sets the state of the shell
 */
void init_shell_state(const char* config_path, const char* cache_path) {
    state.L = luaL_newstate();
    // luaL_openlibs(state.L);
    create_signal_metatable(state.L);
    create_input_key_metatable(state.L);
    lua_newtable(state.L);
    log_debug("setting up plugin manager");
    init_plugin_table();
    // NOTE: maybe rename to bootstrap
    // since it is meant to be just used to prepare core lol
    lua_setfield(state.L, -2, "plugin");
    lua_setglobal(state.L, "lyra");

    log_debug("initing shell variables");
    init_shell_variables();

    log_debug("initing shell config and cache paths");
    init_shell_config(config_path, cache_path);
    init_shell_event_handler();

    char* path = path_get_string(state.config.config);
    log_debug("running init: %s", path);
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
    if (state.vars.term.in_alternate_screen) {
        leave_alternate_screen();
    }
    if (state.vars.term.in_raw_mode) {
        unset_raw_mode();
    }

    const CIteratorString* iter = get_plugin_iterator(state.manager);
    const char* next            = NULL;

    log_debug("unloading plugins");
    while ((next = next_plugin_name(iter)) != NULL) {
        log_debug("unloading: %s", next);
        manager_unload_plugin(state.L, state.manager, next);
    }

    log_debug("closing lua %p", state.L);
    lua_close(state.L);

    log_debug("cleanin shell state");
    free(state.vars.host.data);
    free(state.vars.user.name.data);

    path_destruct(&state.vars.cwd);
    path_destruct(&state.vars.user.home);
    path_destruct(&state.config.config);

    log_debug("deleting pluging manager");
    delete_plugin_manager(state.manager);

    log_debug("removing hooks {}");
    if (state.event.hooks.data != NULL) {
        free(state.event.hooks.data);
        state.event.hooks.data = NULL;
    }
    log_debug("setting state to {}");
    state = (struct ShellState){};
}

void set_to_foreground() {
    log_debug("setting to foreground");
    const int FD = STDIN_FILENO;
    if (tcgetpgrp(FD) < 0)
        temporal_suicide_msg("could not get fd");
    if (setpgid(0, 0) == -1)
        temporal_suicide_msg("could not set program id");
    pid_t group_id = getpgrp();
    // NOTE: I don't know why this is needed
    // it just works like that
    // sure why not ignore this random signal
    signal(SIGTTOU, SIG_IGN);
    int ok = tcsetpgrp(FD, group_id);
    if (ok == -1)
        temporal_suicide_msg("could not set group id");
}

void unset_raw_mode() {
    if (state.vars.term.in_raw_mode) {
        if (state.vars.term.got_original) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.vars.term.orig_termios);
        }
        else
            log_warn("there is no original termios?");
    }
    state.vars.term.in_raw_mode = false;
}

void set_raw_mode() {
    if (!state.vars.term.in_raw_mode) {
        state.vars.term.in_raw_mode = true;
        log_debug("entering raw mode...");
        if (!state.vars.term.got_original) {
            tcgetattr(STDIN_FILENO, &state.vars.term.orig_termios);
            atexit(unset_raw_mode);
            state.vars.term.got_original = true;
        }

        struct termios raw = state.vars.term.orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 1;

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        log_debug("entered raw mode");
    }
}

void enter_alternate_screen() {
    if (!state.vars.term.in_alternate_screen) {
        state.vars.term.in_alternate_screen = true;
        write(STDOUT_FILENO, "\x1b[?1049h", 8);
        log_debug("entered alternate screen");
    }
}

void leave_alternate_screen() {
    if (state.vars.term.in_alternate_screen) {
        state.vars.term.in_alternate_screen = false;
        write(STDOUT_FILENO, "\x1b[?1049l", 8);
        log_debug("left alternate screen");
    }
}
