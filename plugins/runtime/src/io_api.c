#include <errno.h>
#include <lauxlib.h>
#include <lua.h>

#include <io_api.h>
#include <path.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "logs.h"

struct Pipe* lua_check_pipe(lua_State* L, int idx) {
    return (struct Pipe*)luaL_checkudata(L, idx, LUA_PIPE_MT);
}

struct FileHandler* check_file_handler(lua_State* L, int idx) {
    return (struct FileHandler*)luaL_checkudata(L, idx, LUA_IO_FD_MT);
}

static enum OpenMode check_open_flags(lua_State* L, int idx) {
    if (lua_isinteger(L, idx)) {
        return (enum OpenMode)lua_tointeger(L, idx);
    }

    const char* s = luaL_checkstring(L, idx);
    if (s != NULL) {
        enum OpenMode flgs = 0;
        if (strchr(s, 'r') != NULL)
            flgs |= OPEN_MODE_READ;
        if (strchr(s, 'w') != NULL)
            flgs |= OPEN_MODE_WRITE;
        if (strchr(s, '+') != NULL) {
            flgs |= OPEN_MODE_READ;
            flgs |= OPEN_MODE_WRITE;
        }
        if (strchr(s, 'a') != NULL) {
            flgs |= OPEN_MODE_WRITE;
            flgs |= OPEN_MODE_CREATE;
            flgs |= OPEN_MODE_APPEND;
        }
        if (strchr(s, 'w') != NULL) {
            flgs |= OPEN_MODE_CREATE;
            flgs |= OPEN_MODE_TRUNC;
        }
        return flgs;
    }

    return (enum OpenMode)luaL_error(L, "invalid bind type '%s'", s);
}

int lua_fd_stderr(lua_State* L) {
    struct FileHandler* ud = lua_newuserdata(L, sizeof(struct FileHandler));
    *ud                    = stderr_handler();
    luaL_getmetatable(L, LUA_IO_FD_MT);
    lua_setmetatable(L, -2);

    return 1;
}

int lua_fd_stdout(lua_State* L) {
    struct FileHandler* ud = lua_newuserdata(L, sizeof(struct FileHandler));
    *ud                    = stdout_handler();
    luaL_getmetatable(L, LUA_IO_FD_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_fd_open(lua_State* L) {
    struct Path* path   = check_path(L, 1);
    enum OpenMode flags = check_open_flags(L, 2);

    struct FileHandler* ud = lua_newuserdata(L, sizeof(struct FileHandler));

    *ud = file_handler_open(*path, flags);
    luaL_getmetatable(L, LUA_IO_FD_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_fd_close(lua_State* L) {
    struct FileHandler* ud = check_file_handler(L, -1);
    log_debug("closing fd%p", ud);
    file_handler_close(*ud);

    return 0;
}

static int lua_fd_write(lua_State* L) {
    struct FileHandler* fd = check_file_handler(L, 1);
    const char* data       = lua_tostring(L, 2);
    size_t len             = strlen(data);
    if (fd->mode & OPEN_MODE_WRITE)
        write(fd->fd, data, len);

    return 0;
}

static int lua_fd_read(lua_State* L) {
    struct FileHandler* fd = check_file_handler(L, 1);
    size_t len             = lseek(fd->fd, -1, SEEK_END);
    char* buf              = malloc(len);
    if (fd->mode & OPEN_MODE_READ)
        read(fd->fd, buf, len);

    lua_pushstring(L, buf);
    return 1;
}

static int lua_fd_get_fd(lua_State* L) {
    struct FileHandler* fd = check_file_handler(L, 1);
    lua_pushinteger(L, fd->fd);
    return 1;
}

/* ---------- Pipe ---------- */

#define BUFFER_LEN 256

static int lua_pipe_new(lua_State* L) {
    struct Pipe* p = (struct Pipe*)lua_newuserdata(L, sizeof(struct Pipe));
    *p             = pipe_new();

    luaL_getmetatable(L, LUA_PIPE_MT);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_pipe_close(lua_State* L) {
    struct Pipe* p = lua_check_pipe(L, 1);
    pipe_close(p);
    return 0;
}

static int lua_pipe_gc(lua_State* L) {
    struct Pipe* p = lua_check_pipe(L, 1);
    log_debug("cleaing pipe %p", p);
    pipe_close(p);
    return 0;
}

static int lua_pipe_read(lua_State* L) {
    struct Pipe* p    = lua_check_pipe(L, 1);
    struct String str = pipe_read(p);
    char* s           = string_to_cstring(str);
    lua_pushstring(L, s);

    free(s);
    free(str.data);

    return 1;
}

static int lua_pipe_write(lua_State* L) {
    struct Pipe* p     = lua_check_pipe(L, 1);
    const char* s      = lua_tostring(L, 2);
    struct String data = string_from_cstr(s);
    pipe_write(p, data);
    free(data.data);

    return 0;
}

static const luaL_Reg pipe_methods[] = {
    {"close", lua_pipe_close},
    {"write", lua_pipe_write},
    { "read",  lua_pipe_read},
    {   NULL,           NULL}
};

static const luaL_Reg pipe_meta[] = {
    {"__gc", lua_pipe_gc},
    {  NULL,        NULL}
};

static const luaL_Reg fd_methods[] = {
    {  "open",   lua_fd_open},
    { "close",  lua_fd_close},
    { "write",  lua_fd_write},
    {  "read",   lua_fd_read},
    {"get_fd", lua_fd_get_fd},
    {    NULL,          NULL},
};

static const luaL_Reg fd_meta[] = {
    {"get_fd", lua_fd_close},
    {    NULL,         NULL},
};

// setup

static void create_pipe_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_PIPE_MT)) {
        luaL_setfuncs(L, pipe_meta, 0);

        lua_newtable(L);
        luaL_setfuncs(L, pipe_methods, 0);
        lua_setfield(L, -2, "__index");
    }

    lua_pop(L, 1);
}

static void create_fd_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_IO_FD_MT)) {
        luaL_setfuncs(L, fd_meta, 0);

        lua_newtable(L);
        luaL_setfuncs(L, fd_methods, 0);
        lua_setfield(L, -2, "__index");
    }
    lua_pop(L, 1);
}

static void push_io_module(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_pipe_new);
    lua_setfield(L, -2, "pipe");

    lua_pushcfunction(L, lua_fd_stderr);
    lua_setfield(L, -2, "stderr");

    lua_pushcfunction(L, lua_fd_stdout);
    lua_setfield(L, -2, "stdout");

    lua_pushcfunction(L, lua_fd_open);
    lua_setfield(L, -2, "open");
}

void io_setup_lua_api(lua_State* L) {
    create_fd_metatable(L);
    create_pipe_metatable(L);

    lua_getglobal(L, "lyra");

    lua_getfield(L, -1, "core");
    lua_getfield(L, -1, "api");

    push_io_module(L);
    lua_setfield(L, -2, "io");

    lua_pop(L, 3);
}
