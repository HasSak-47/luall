#ifndef __IO_API_H__
#define __IO_API_H__

#include <lua.h>
#include <path_api.h>

#include <stdbool.h>

#define LUA_PIPE_MT "lyra.core.api.io.pipe"
#define LUA_IO_FD_MT "lyra.core.api.io.fd"
#define LUA_IO_MODE_MT "lyra.core.api.io.mode"

struct Pipe {
    int p[2];
};

struct Pipe pipe_new();
void pipe_close(struct Pipe* p);

struct String pipe_read(struct Pipe* o);
void pipe_write(struct Pipe* o, struct String data);

enum OpenMode {
    OPEN_MODE_CREATE = 4,
    OPEN_MODE_READ   = 1,
    OPEN_MODE_WRITE  = 2,
};

struct FileHandler {
    int fd;
    enum OpenMode mode;
    bool should_close;
};

void io_setup_lua_api(lua_State* L);

struct FileHandler stdout_handler();
struct FileHandler stderr_handler();
struct FileHandler file_handler_open(struct Path path, enum OpenMode flags);
void file_handler_close(struct FileHandler file);

struct Pipe* lua_check_pipe(lua_State* L, int idx);
struct FileHandler* lua_check_file(lua_State* L, int idx);

#endif
