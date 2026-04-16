#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>

#include "ly_string.h"
#include "process.h"

#define LUA_COMMAND_MT "rewsh.api.process.command"
#define LUA_PIPE_MT "rewsh.api.process.pipe"

static struct Command* check_command(lua_State* L, int idx) {
    return (struct Command*)luaL_checkudata(L, idx, LUA_COMMAND_MT);
}

static struct Pipe* check_pipe(lua_State* L, int idx) {
    return (struct Pipe*)luaL_checkudata(L, idx, LUA_PIPE_MT);
}

static enum BindType lua_check_bind_type(lua_State* L, int idx) {
    if (lua_isinteger(L, idx)) {
        return (enum BindType)lua_tointeger(L, idx);
    }

    const char* s = luaL_checkstring(L, idx);
    if (strcmp(s, "read") == 0)
        return ReadBind;
    if (strcmp(s, "write") == 0)
        return WriteBind;
    if (strcmp(s, "error") == 0)
        return ErrorBind;
    if (strcmp(s, "none") == 0)
        return NoneBind;

    return (enum BindType)luaL_error(L, "invalid bind type '%s'", s);
}

/* ---------- Pipe ---------- */

static int lua_pipe_new(lua_State* L) {
    struct Pipe* p = (struct Pipe*)lua_newuserdata(L, sizeof(struct Pipe));
    *p             = new_pipe();

    luaL_getmetatable(L, LUA_PIPE_MT);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_pipe_close(lua_State* L) {
    struct Pipe* p = check_pipe(L, 1);
    close_pipe(p);
    return 0;
}

static int lua_pipe_gc(lua_State* L) {
    struct Pipe* p = check_pipe(L, 1);
    close_pipe(p);
    return 0;
}

static int lua_pipe_read(lua_State* L) {
    struct Pipe* p    = check_pipe(L, 1);
    struct String str = read_pipe(p);
    char* s           = string_to_cstring(str);
    lua_pushstring(L, s);

    free(s);
    free(str.data);

    return 0;
}

static int lua_pipe_write(lua_State* L) {
    struct Pipe* p     = check_pipe(L, 1);
    const char* s      = lua_tostring(L, 2);
    struct String data = string_from_cstr(s);
    write_pipe(p, data);
    free(data.data);

    return 0;
}

/* ---------- Command ---------- */

static int lua_command_new(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    struct Command* cmd =
        (struct Command*)lua_newuserdata(L, sizeof(struct Command));
    *cmd = new_command(path);

    luaL_getmetatable(L, LUA_COMMAND_MT);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_command_reserve_size(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    size_t size         = (size_t)luaL_checkinteger(L, 2);
    command_reserve_size(cmd, size);
    return 0;
}

static int lua_command_add_arg(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    const char* arg     = luaL_checkstring(L, 2);
    add_arg(cmd, arg);
    return 0;
}

static int lua_command_bind_pipe(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    struct Pipe* pipe   = check_pipe(L, 2);
    enum BindType ty    = lua_check_bind_type(L, 3);

    bind_pipe(cmd, pipe, ty);
    return 0;
}

static int lua_command_run(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    pid_t pid           = run(cmd);
    lua_pushinteger(L, (lua_Integer)pid);
    return 1;
}

static int lua_command_wait(lua_State* L) {
    struct Command* cmd = (struct Command*)check_command(L, 1);
    if (cmd->running_pid == 0) {
        luaL_error(L, "command not running");
        return 1;
    }
    int status = wait_process(cmd->running_pid);
    lua_pushinteger(L, (lua_Integer)status);
    return 1;
}

static int lua_process_wait(lua_State* L) {
    pid_t pid  = (pid_t)luaL_checkinteger(L, 1);
    int status = wait_process(pid);
    lua_pushinteger(L, (lua_Integer)status);
    return 1;
}

static int lua_command_set_foreground(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    cmd->foreground     = lua_toboolean(L, 2);
    return 0;
}

static int lua_command_get_foreground(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    lua_pushboolean(L, cmd->foreground);
    return 1;
}

static int lua_command_gc(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    (void)cmd;
    return 0;
}

/* ---------- method tables ---------- */

static const luaL_Reg pipe_methods[] = {
    {"close", lua_pipe_close},
    {"write", lua_pipe_write},
    { "read",  lua_pipe_read},
    {"close", lua_pipe_close},
    {   NULL,           NULL}
};

static const luaL_Reg pipe_meta[] = {
    {"__gc", lua_pipe_gc},
    {  NULL,        NULL}
};

static const luaL_Reg command_methods[] = {
    {  "reserve_size",   lua_command_reserve_size},
    {       "add_arg",        lua_command_add_arg},
    {     "bind_pipe",      lua_command_bind_pipe},
    {           "run",            lua_command_run},
    {          "wait",           lua_command_wait},
    {"set_foreground", lua_command_set_foreground},
    {"get_foreground", lua_command_get_foreground},
    {            NULL,                       NULL}
};

static const luaL_Reg command_meta[] = {
    {"__gc", lua_command_gc},
    {  NULL,           NULL}
};

/* ---------- install helpers ---------- */

static void create_command_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_COMMAND_MT)) {
        luaL_setfuncs(L, command_meta, 0);

        lua_newtable(L);
        luaL_setfuncs(L, command_methods, 0);
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);
}

static void create_pipe_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_PIPE_MT)) {
        luaL_setfuncs(L, pipe_meta, 0);

        lua_newtable(L);
        luaL_setfuncs(L, pipe_methods, 0);
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);
}

static void push_process_module(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_command_new);
    lua_setfield(L, -2, "command");

    lua_pushcfunction(L, lua_pipe_new);
    lua_setfield(L, -2, "pipe");

    lua_pushcfunction(L, lua_process_wait);
    lua_setfield(L, -2, "wait");

    lua_pushinteger(L, NoneBind);
    lua_setfield(L, -2, "NONE");

    lua_pushinteger(L, ReadBind);
    lua_setfield(L, -2, "READ");

    lua_pushinteger(L, WriteBind);
    lua_setfield(L, -2, "WRITE");

    lua_pushinteger(L, ErrorBind);
    lua_setfield(L, -2, "ERROR");
}

/* ---------- public setup ---------- */

void process_setup_lua_api(lua_State* L) {
    create_command_metatable(L);
    create_pipe_metatable(L);

    /* stack: empty */

    lua_getglobal(L, "rewsh");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        luaL_error(L, "global 'rewsh' does not exist or is not a table");
        return;
    }

    /* stack: rewsh */

    lua_getfield(L, -1, "api");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);        /* pop old api/nil */
        lua_newtable(L);      /* create rewsh.api */
        lua_pushvalue(L, -1); /* duplicate for setfield */
        lua_setfield(L, -3, "api");
    }

    /* stack: rewsh, api */

    push_process_module(L);
    lua_setfield(L, -2, "process"); /* api.process = module */

    /* stack: rewsh, api */
    lua_pop(L, 2);
}
