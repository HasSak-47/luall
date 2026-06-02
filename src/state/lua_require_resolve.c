#include <lauxlib.h>
#include <lua.h>

#include <bindgen_plugin.h>
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

int register_plugin_module_paths(lua_State* L, const char* plugin_path) {
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
