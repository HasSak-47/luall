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

struct FDHandler stdout_handler() {
    return (struct FDHandler){
        STDOUT_FILENO,
        IO_MODE_WRITE,
        false,
    };
}

struct FDHandler stderr_handler() {
    return (struct FDHandler){
        STDERR_FILENO,
        IO_MODE_WRITE,
        false,
    };
}

struct FDHandler file_handler(struct Path path, enum IoMode flags) {
    char* file = path_get_string(path);
    int o_flag = (flags & IO_MODE_CREATE ? O_CREAT : 0);
    // yo unix wtf
    if ((flags & IO_MODE_READ) && (flags & IO_MODE_WRITE)) {
        o_flag += O_RDWR;
    }
    else if ((flags & IO_MODE_READ)) {
        o_flag += O_RDONLY;
    }
    else if ((flags & IO_MODE_WRITE)) {
        o_flag += O_WRONLY;
    }
    int mask = umask(0);
    umask(mask);

    int fd = open(file, o_flag, 0666 & ~mask);
    return (struct FDHandler){
        fd,
        true,
    };
}

void handler_destroy(struct FDHandler handler) {
    if (handler.should_close) {
        close(handler.fd);
    }
}

static struct FDHandler* check_fdhanler(lua_State* L, int idx) {
    return (struct FDHandler*)luaL_checkudata(L, idx, LUA_IO_FD_MT);
}

static enum IoMode check_open_flags(lua_State* L, int idx) {
    if (lua_isinteger(L, idx)) {
        return (enum IoMode)lua_tointeger(L, idx);
    }

    const char* s = luaL_checkstring(L, idx);
    if (s != NULL) {
        enum IoMode flgs = 0;
        if (strchr(s, 'r') != NULL)
            flgs |= IO_MODE_READ;
        if (strchr(s, 'w') == 0)
            flgs |= IO_MODE_WRITE;
        if (strchr(s, '+') == 0)
            flgs |= IO_MODE_CREATE;
        return flgs;
    }

    return (enum IoMode)luaL_error(L, "invalid bind type '%s'", s);
}

int lua_fd_stderr(lua_State* L) {
    struct FDHandler* ud = lua_newuserdata(L, sizeof(struct FDHandler));
    *ud                  = stderr_handler();
    luaL_getmetatable(L, LUA_IO_FD_MT);
    lua_setmetatable(L, -2);

    return 1;
}

int lua_fd_stdout(lua_State* L) {
    struct FDHandler* ud = lua_newuserdata(L, sizeof(struct FDHandler));
    *ud                  = stdout_handler();
    luaL_getmetatable(L, LUA_IO_FD_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_fd_open(lua_State* L) {
    struct Path* path = check_path(L, 1);
    enum IoMode flags = check_open_flags(L, 2);

    struct FDHandler* ud = lua_newuserdata(L, sizeof(struct FDHandler));

    *ud = file_handler(*path, flags);
    luaL_getmetatable(L, LUA_IO_FD_MT);
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_fd_close(lua_State* L) {
    struct FDHandler* ud = check_fdhanler(L, -1);
    log_debug("closing fd%p", ud);
    handler_destroy(*ud);

    return 0;
}

static int lua_fd_write(lua_State* L) {
    struct FDHandler* fd = check_fdhanler(L, 1);
    const char* data     = lua_tostring(L, 2);
    size_t len           = strlen(data);
    if (fd->mode & IO_MODE_READ)
        write(fd->fd, data, len);

    return 0;
}

static int lua_fd_read(lua_State* L) {
    struct FDHandler* fd = check_fdhanler(L, 1);
    size_t len           = lseek(fd->fd, -1, SEEK_END);
    char* buf            = malloc(len);
    if (fd->mode & IO_MODE_READ)
        read(fd->fd, buf, len);

    lua_pushstring(L, buf);
    return 1;
}

static int lua_fd_get_fd(lua_State* L) {
    struct FDHandler* fd = check_fdhanler(L, 1);
    lua_pushinteger(L, fd->fd);
    return 1;
}

static const luaL_Reg fd_methods[] = {
    {  "open",   lua_fd_open},
    { "close",  lua_fd_close},
    { "write",  lua_fd_write},
    {  "read",   lua_fd_read},
    {"get_fd", lua_fd_get_fd},
    {  "__gc",  lua_fd_close},
    {    NULL,          NULL},
};

static void push_io_module(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, lua_fd_stderr);
    lua_setfield(L, -2, "stderr");

    lua_pushcfunction(L, lua_fd_stdout);
    lua_setfield(L, -2, "stdout");

    lua_pushcfunction(L, lua_fd_open);
    lua_setfield(L, -2, "open");
}

void io_setup_lua_api(lua_State* L) {
    if (luaL_newmetatable(L, LUA_IO_FD_MT)) {
        luaL_setfuncs(L, fd_methods, 0);

        lua_newtable(L);
        luaL_setfuncs(L, fd_methods, 0);
        lua_setfield(L, -2, "__index");
    }

    lua_getglobal(L, "lyra");

    lua_getfield(L, -1, "core");
    lua_getfield(L, -1, "api");

    push_io_module(L);
    lua_setfield(L, -2, "io");
    lua_pop(L, 3);
}
