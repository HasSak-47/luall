#ifndef __IO_API_H__
#define __IO_API_H__

#include <lua.h>
#include <path_api.h>

#include <stdbool.h>

#define LUA_IO_FD_MT "lyra.api.io.fd"
#define LUA_IO_MODE_MT "lyra.api.io.mode"

enum IoMode {
    IO_MODE_CREATE = 4,
    IO_MODE_READ   = 1,
    IO_MODE_WRITE  = 2,
};

struct FDHandler {
    int fd;
    enum IoMode mode;
    bool should_close;
};

void io_setup_lua_api(lua_State* L);

struct FDHandler stdout_handler();
struct FDHandler stderr_handler();
struct FDHandler file_handler(struct Path path, enum IoMode flags);

#endif
