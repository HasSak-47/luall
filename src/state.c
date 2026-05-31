#include <lauxlib.h>
#include <lua.h>

#include <logs.h>
#include <ly_string.h>
#include <path.h>
#include <plugin.h>
#include <plugin/definitions.h>
#include <state.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>

struct ShellState state = {};

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

static int lua_plugin_api_resolve(lua_State* L) {
    const char* path = lua_tostring(L, 1);
    manager_resolve_plugin(state.manager, path);
    return 0;
}

static int lua_plugin_api_prepare(lua_State* L) {
    const char* path = lua_tostring(L, 1);
    manager_prepare_plugin(state.manager, path);
    return 0;
}

static int push_plugin_config(lua_State* L, const char* path, int opts_idx) {
    if (!lua_isnoneornil(L, opts_idx)) {
        if (!lua_istable(L, opts_idx)) {
            return luaL_error(L, "plugin require opts must be a table or nil");
        }

        lua_pushvalue(L, opts_idx);
        return 1;
    }

    lua_getglobal(L, "lyra");
    lua_getfield(L, -1, "plugin");
    lua_getfield(L, -1, "config");
    lua_pushstring(L, path);
    lua_gettable(L, -2);
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_remove(L, -2);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        return 1;
    }

    if (!lua_istable(L, -1)) {
        return luaL_error(
            L, "plugin config for '%s' must be a table or nil", path);
    }

    return 1;
}

static int package_append_path(
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

    lua_pushfstring(L, "%s%s%s", current ? current : "",
        current && current[0] ? ";" : "", entry);
    lua_setfield(L, -3, field);
    lua_pop(L, 2);
    return 0;
}

static char* plugin_namespace_from_path(const char* plugin_path) {
    const char* name = strrchr(plugin_path, '/');
    if (name == NULL) {
        name = plugin_path;
    }
    else {
        name += 1;
    }

    size_t len = strlen(name);
    char* out  = malloc(len + 1);
    memcpy(out, name, len + 1);
    return out;
}

static char* module_suffix_to_path(const char* module_suffix) {
    size_t len = strlen(module_suffix);
    char* out  = malloc(len + 1);
    for (size_t i = 0; i < len; ++i) {
        out[i] = module_suffix[i] == '.' ? '/' : module_suffix[i];
    }
    out[len] = '\0';
    return out;
}

static int caller_is_within_plugin(lua_State* L, const char* plugin_path) {
    lua_Debug debug           = {};
    size_t plugin_path_length = strlen(plugin_path);

    for (int level = 0; lua_getstack(L, level, &debug); ++level) {
        if (!lua_getinfo(L, "S", &debug)) {
            continue;
        }

        if (debug.source == NULL || debug.source[0] != '@') {
            continue;
        }

        const char* source_path = debug.source + 1;
        if (strncmp(source_path, plugin_path, plugin_path_length) != 0) {
            continue;
        }

        if (source_path[plugin_path_length] == '\0' ||
            source_path[plugin_path_length] == '/') {
            return 1;
        }
    }

    return 0;
}

static int push_plugin_module_loader(
    lua_State* L, const char* plugin_path, const char* module_suffix) {
    char* relative_path = module_suffix_to_path(module_suffix);
    size_t plugin_len   = strlen(plugin_path);
    size_t relative_len = strlen(relative_path);

    const char* root_file_suffix = "/init.lua";
    const char* lua_file_suffix  = "/lua/";
    const char* file_suffix      = ".lua";
    const char* init_suffix      = "/init.lua";

    char* root_init = malloc(plugin_len + strlen(root_file_suffix) + 1);
    snprintf(root_init, plugin_len + strlen(root_file_suffix) + 1, "%s%s",
        plugin_path, root_file_suffix);

    char* lua_file = malloc(plugin_len + strlen(lua_file_suffix) +
                            relative_len + strlen(file_suffix) + 1);
    snprintf(lua_file,
        plugin_len + strlen(lua_file_suffix) + relative_len +
            strlen(file_suffix) + 1,
        "%s%s%s%s", plugin_path, lua_file_suffix, relative_path, file_suffix);

    char* lua_init = malloc(plugin_len + strlen(lua_file_suffix) +
                            relative_len + strlen(init_suffix) + 1);
    snprintf(lua_init,
        plugin_len + strlen(lua_file_suffix) + relative_len +
            strlen(init_suffix) + 1,
        "%s%s%s%s", plugin_path, lua_file_suffix, relative_path, init_suffix);

    int status = LUA_ERRFILE;
    if (module_suffix[0] == '\0' && access(root_init, R_OK) == 0) {
        status = luaL_loadfile(L, root_init);
    }
    else if (access(lua_file, R_OK) == 0) {
        status = luaL_loadfile(L, lua_file);
    }
    else if (access(lua_init, R_OK) == 0) {
        status = luaL_loadfile(L, lua_init);
    }

    free(relative_path);
    free(root_init);
    free(lua_file);
    free(lua_init);

    if (status == LUA_OK) {
        return 1;
    }
    if (status != LUA_ERRFILE) {
        return lua_error(L);
    }

    return 0;
}

static int plugin_namespace_searcher(lua_State* L) {
    const char* module_name   = luaL_checkstring(L, 1);
    const char* plugin_name   = lua_tostring(L, lua_upvalueindex(1));
    const char* plugin_path   = lua_tostring(L, lua_upvalueindex(2));
    size_t plugin_name_length = strlen(plugin_name);

    if (strncmp(module_name, plugin_name, plugin_name_length) != 0) {
        lua_pushfstring(L, "\n\tno plugin namespace '%s' for module '%s'",
            plugin_name, module_name);
        return 1;
    }

    const char* module_suffix = module_name + plugin_name_length;
    if (module_suffix[0] == '\0') {
        module_suffix = "";
    }
    else if (module_suffix[0] == '.') {
        module_suffix += 1;
    }
    else {
        lua_pushfstring(L, "\n\tno plugin namespace '%s' for module '%s'",
            plugin_name, module_name);
        return 1;
    }

    if (!caller_is_within_plugin(L, plugin_path)) {
        lua_pushfstring(L,
            "\n\tmodule '%s' is private to plugin namespace '%s'", module_name,
            plugin_name);
        return 1;
    }

    if (push_plugin_module_loader(L, plugin_path, module_suffix)) {
        return 1;
    }

    lua_pushfstring(L,
        "\n\tno file '%s/init.lua'\n\tno file '%s/lua/%s.lua'\n\tno file "
        "'%s/lua/%s/init.lua'",
        plugin_path, plugin_path, module_suffix, plugin_path, module_suffix);
    return 1;
}

static int register_plugin_namespace_searcher(
    lua_State* L, const char* plugin_name, const char* plugin_path) {
    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return luaL_error(L, "package table is not available");
    }

    lua_getfield(L, -1, "_lyra_plugin_namespaces");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "_lyra_plugin_namespaces");
    }

    lua_getfield(L, -1, plugin_name);
    if (lua_toboolean(L, -1)) {
        lua_pop(L, 3);
        return 0;
    }
    lua_pop(L, 1);

    lua_pushboolean(L, 1);
    lua_setfield(L, -2, plugin_name);
    lua_pop(L, 1);

    lua_getfield(L, -1, "searchers");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return luaL_error(L, "package.searchers is not available");
    }

    lua_pushstring(L, plugin_name);
    lua_pushstring(L, plugin_path);
    lua_pushcclosure(L, plugin_namespace_searcher, 2);
    lua_seti(L, -2, lua_rawlen(L, -2) + 1);

    lua_pop(L, 2);
    return 0;
}

static int register_plugin_module_paths(lua_State* L, const char* plugin_path) {
    char* plugin_name           = plugin_namespace_from_path(plugin_path);
    size_t base_len             = strlen(plugin_path);
    const char* lua_suffix      = "/lua/?.lua";
    const char* lua_init_suffix = "/lua/?/init.lua";

    char* lua_path      = malloc(base_len + strlen(lua_suffix) + 1);
    char* lua_init_path = malloc(base_len + strlen(lua_init_suffix) + 1);

    snprintf(lua_path, base_len + strlen(lua_suffix) + 1, "%s%s", plugin_path,
        lua_suffix);
    snprintf(lua_init_path, base_len + strlen(lua_init_suffix) + 1, "%s%s",
        plugin_path, lua_init_suffix);

    int status =
        register_plugin_namespace_searcher(L, plugin_name, plugin_path);
    if (status == 0) {
        status = package_append_path(L, "path", lua_init_path);
    }
    if (status == 0) {
        status = package_append_path(L, "path", lua_path);
    }

    free(plugin_name);
    free(lua_path);
    free(lua_init_path);
    return status;
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
    case PLUGIN_KIND_BINARY:
        push_plugin_config(L, path, 2);
        h->setup(L);
        break;

    case PLUGIN_KIND_LUA:
        if (h->setup_reference != 0) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, h->setup_reference);
            return 1;
        }

        if (register_plugin_module_paths(L, h->lua_path) != 0) {
            return lua_error(L);
        }

        lua_pushfstring(L, "%s/init.lua", h->lua_path);
        const char* entrypoint = lua_tostring(L, -1);
        if (luaL_loadfile(L, entrypoint) != LUA_OK) {
            lua_remove(L, -2);
            return lua_error(L);
        }
        lua_remove(L, -2);

        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            return lua_error(L);
        }

        if (!lua_istable(L, -1)) {
            return luaL_error(L,
                "failed to require lua plugin '%s': entrypoint must return a table",
                path);
        }

        lua_getfield(L, -1, "unload");
        if (lua_isfunction(L, -1)) {
            h->destruct_reference = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        else {
            lua_pop(L, 1);
        }

        lua_getfield(L, -1, "setup");
        if (lua_isfunction(L, -1)) {
            push_plugin_config(L, path, 2);

            if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                return lua_error(L);
            }

            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
            }
            else if (!lua_istable(L, -1)) {
                return luaL_error(L,
                    "failed to require lua plugin '%s': setup must return a table "
                    "or nil",
                    path);
            }
        }
        else {
            lua_pop(L, 1);
        }

        if (lua_gettop(L) == 0 || !lua_istable(L, -1)) {
            return luaL_error(L,
                "failed to require lua plugin '%s': no exports table produced",
                path);
        }

        h->setup_reference = luaL_ref(L, LUA_REGISTRYINDEX);
        lua_rawgeti(L, LUA_REGISTRYINDEX, h->setup_reference);
        break;
    }

    return 1;
}

static int lua_plugin_api_destroy(lua_State* L) {
    return 0;
}

void init_plugin_table() {
    lua_newtable(state.L);

    lua_pushcfunction(state.L, lua_plugin_api_resolve);
    lua_setfield(state.L, -2, "resolve");

    lua_pushcfunction(state.L, lua_plugin_api_prepare);
    lua_setfield(state.L, -2, "prepare");

    lua_pushcfunction(state.L, lua_plugin_api_require);
    lua_setfield(state.L, -2, "require");

    lua_pushcfunction(state.L, lua_plugin_api_destroy);
    lua_setfield(state.L, -2, "destroy");

    lua_newtable(state.L);
    lua_setfield(state.L, -2, "config");
}

/**
 * sets the state of the shell
 */
void init_shell_state(const char* config_path, const char* cache_path) {
    state.L = luaL_newstate();
    // luaL_openlibs(state.L);
    create_input_key_metatable(state.L);
    lua_newtable(state.L);
    init_plugin_table();
    // NOTE: maybe rename to bootstrap
    // since it is meant to be just used to prepare core lol
    lua_setfield(state.L, -2, "plugin");
    lua_pushlightuserdata(state.L, &state);
    lua_setfield(state.L, -2, "state");

    lua_setglobal(state.L, "lyra");

    init_shell_variables();
    init_shell_config(config_path, cache_path);

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
    const CIteratorString* iter = get_plugin_iterator(state.manager);
    const char* next            = NULL;

    log_debug("unloading plugins");
    while ((next = next_plugin_name(iter)) != NULL) {
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
    if (state.hooks.data != NULL) {
        free(state.hooks.data);
        state.hooks.data = NULL;
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
    state.vars.term.in_raw_mode = false;
    if (state.vars.term.got_original)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &state.vars.term.orig_termios);
    else
        log_warn("there is no original termios?");
}

void set_raw_mode() {
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
