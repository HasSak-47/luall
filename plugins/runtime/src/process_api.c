#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>

#include <ly_string.h>
#include <process.h>
#include "logs.h"

#define LUA_COMMAND_MT "lyra.core.api.process.command"
#define LUA_PIPE_MT "lyra.core.api.io.pipe"
#define LUA_IO_FD_MT "lyra.core.api.io.fd"

static struct Command* check_command(lua_State* L, int idx) {
    return (struct Command*)luaL_checkudata(L, idx, LUA_COMMAND_MT);
}

static int lua_command_new(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);

    struct Command* cmd =
        (struct Command*)lua_newuserdata(L, sizeof(struct Command));
    *cmd = command_new(path);

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
    command_add_arg(cmd, arg);
    return 0;
}

static int lua_command_bind(lua_State* L) {
    struct Command* cmd       = check_command(L, 1);
    enum ProcessBindKind kind = (enum ProcessBindKind)luaL_checkinteger(L, 3);
    struct ProcessBind bind   = {};

    if (luaL_testudata(L, 2, LUA_PIPE_MT) != NULL) {
        log_debug("binding pipe to process");
        bind = bind_from_pipe((struct Pipe*)lua_touserdata(L, 2));
    }
    else if (luaL_testudata(L, 2, LUA_IO_FD_MT) != NULL) {
        log_debug("binding file to process");
        bind = bind_from_file((struct FileHandler*)lua_touserdata(L, 2));
    }
    else if (lua_istable(L, 2)) {
        lua_pushvalue(L, 2);
        log_debug("binding lua object to process");
        bind = bind_from_lua((struct LuaBind){
            .reference = luaL_ref(L, LUA_REGISTRYINDEX),
        });
    }
    else {
        return luaL_error(L, "expected pipe, file, or table");
    }

    bind.kind       = kind;
    cmd->binded_in  = cmd->binded_in || (kind & PROCESS_READ) != 0;
    cmd->binded_out = cmd->binded_out || (kind & PROCESS_WRITE) != 0;
    cmd->binded_err = cmd->binded_err || (kind & PROCESS_ERROR) != 0;
    command_bind_io(cmd, bind);

    return 0;
}

static int lua_command_run(lua_State* L) {
    struct Command* cmd = check_command(L, 1);
    pid_t pid           = command_run(cmd);
    lua_pushinteger(L, (lua_Integer)pid);
    return 1;
}

static int lua_command_wait(lua_State* L) {
    struct Command* cmd = (struct Command*)check_command(L, 1);
    if (cmd->running_pid == 0) {
        luaL_error(L, "command not running");
        return 1;
    }
    int status = process_wait(cmd->running_pid);
    lua_pushinteger(L, (lua_Integer)status);
    return 1;
}

static int lua_process_wait(lua_State* L) {
    pid_t pid  = (pid_t)luaL_checkinteger(L, 1);
    int status = process_wait(pid);
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
    log_debug("cleaing cmd%p", cmd);

    for (size_t i = 0; i < cmd->bind.len; ++i) {
        struct ProcessBind* bind = &cmd->bind.data[i];
        bind->vt->delete(bind->handler);
    }

    free(cmd->bind.data);
    cmd->bind.data = NULL;
    cmd->bind.len  = 0;
    cmd->bind.cap  = 0;

    if (cmd->cmd != NULL) {
        for (size_t i = 0; i < cmd->args.len; ++i) {
            if (cmd->args.data[i] == NULL) {
                break;
            }

            free(cmd->args.data[i]);
        }

        free(cmd->args.data);
        free(cmd->cmd);
        cmd->args.data = NULL;
        cmd->args.len  = 0;
        cmd->args.cap  = 0;
        cmd->cmd       = NULL;
    }

    return 0;
}

/* ---------- method tables ---------- */

static const luaL_Reg command_methods[] = {
    {  "reserve_size",   lua_command_reserve_size},
    {          "bind",           lua_command_bind},
    {       "add_arg",        lua_command_add_arg},
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

static void push_process_module(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_command_new);
    lua_setfield(L, -2, "command");

    lua_pushcfunction(L, lua_process_wait);
    lua_setfield(L, -2, "wait");

    lua_pushinteger(L, PROCESS_NONE);
    lua_setfield(L, -2, "NONE");

    lua_pushinteger(L, PROCESS_READ);
    lua_setfield(L, -2, "READ");

    lua_pushinteger(L, PROCESS_WRITE);
    lua_setfield(L, -2, "WRITE");

    lua_pushinteger(L, PROCESS_ERROR);
    lua_setfield(L, -2, "ERROR");
}

/* ---------- public setup ---------- */

void process_setup_lua_api(lua_State* L) {
    create_command_metatable(L);

    lua_getglobal(L, "lyra");

    lua_getfield(L, -1, "core");
    lua_getfield(L, -1, "api");

    push_process_module(L);
    lua_setfield(L, -2, "process"); /* api.process = module */

    lua_pop(L, 3);
}
