#include <lauxlib.h>
#include <lua.h>

#include <debug.h>
#include <ly_string.h>
#include <path.h>
#include <path_api.h>

#include <stdio.h>
#include <stdlib.h>
#include "logs.h"

struct Path* check_path(lua_State* L, int idx) {
    return (struct Path*)luaL_checkudata(L, idx, LUA_PATH_MT);
}

static int lua_new_path(lua_State* L) {
    struct Path* path = (struct Path*)lua_newuserdata(L, sizeof(struct Path));
    // WARN: leaky interface
    (path->_inner) = (struct InnerVectorPath){0, 0, 0};
    luaL_getmetatable(L, LUA_PATH_MT);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_get_path_string(lua_State* L) {
    struct Path* path = check_path(L, 1);
    char* c           = path_get_string(*path);
    lua_pushstring(L, c);
    free(c);

    return 1;
}
static int lua_push_name(lua_State* L) {
    struct Path* path = check_path(L, 1);
    const char* name  = lua_tostring(L, 2);
    path_push_name(path, name);
    return 0;
}

static int lua_path_is_dir(lua_State* L) {
    struct Path* path = check_path(L, 1);
    lua_pushboolean(L, path_is_dir(path));

    return 1;
}

static int lua_pop_segment(lua_State* L) {
    struct Path* path = check_path(L, 1);
    path_pop_segment(path);
    return 0;
}
static int lua_expand_path(lua_State* L) {
    struct Path* path = check_path(L, 1);
    struct Path* cwd  = check_path(L, 2);
    path_expand(path, cwd);
    return 0;
}

static int lua_parse_path(lua_State* L) {
    const char* str_path = lua_tostring(L, 1);
    struct Path* path    = lua_newuserdata(L, sizeof(struct Path));
    *path                = path_parse(str_path);
    luaL_getmetatable(L, LUA_PATH_MT);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_get_childs(lua_State* L) {
    struct Path* path        = check_path(L, 1);
    struct VectorPath childs = path_get_childs(path);

    lua_createtable(L, childs.len, childs.len);
    for (size_t i = 0; i < childs.len; ++i) {
        struct Path* p = lua_newuserdata(L, sizeof(struct Path));
        *p             = childs.data[i];
        lua_seti(L, -2, i + 1);
    }

    return 1;
}
/*
 * returns a copy of the last path segment if it is a named type, empty string
 * if it is not
 */
static int lua_get_name(lua_State* L) {
    struct Path* path  = check_path(L, 1);
    struct String name = path_get_name(path);
    char* name_str     = string_to_cstring(name);
    lua_pushstring(L, name_str);
    free(name_str);
    free(name.data);

    return 1;
}

static int lua_path_gc(lua_State* L) {
    struct Path* path = check_path(L, -1);
    log_debug("cleaing cmd %p", path);
    path_destruct(path);
    return 0;
}

static const luaL_Reg path_methods[] = {
    {  "to_string", lua_get_path_string},
    {       "push",       lua_push_name},
    {        "pop",     lua_pop_segment},
    {"path_is_dir",     lua_path_is_dir},
    {"expand_path",     lua_expand_path},
    { "get_childs",      lua_get_childs},
    {   "get_name",        lua_get_name},
    {         NULL,                NULL}
};

static const luaL_Reg path_meta[] = {
    {"__gc", lua_path_gc},
    {  NULL,        NULL}
};

void create_path_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_PATH_MT)) {
        luaL_setfuncs(L, path_meta, 0);
        lua_newtable(L);
        luaL_setfuncs(L, path_methods, 0);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);
}

void create_path_module(lua_State* L) {
    lua_createtable(L, 0, 0);
    lua_pushcfunction(L, lua_parse_path);
    lua_setfield(L, -2, "parse");
    lua_pushcfunction(L, lua_new_path);
    lua_setfield(L, -2, "new");
}

void path_setup_lua_api(lua_State* L) {
    create_path_metatable(L);

    lua_getglobal(L, "lyra");
    lua_getfield(L, -1, "core");
    lua_getfield(L, -1, "api");

    create_path_module(L);
    lua_setfield(L, -2, "path");

    lua_pop(L, 3);
}
