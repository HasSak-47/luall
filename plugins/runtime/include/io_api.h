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

enum BindType {
    NoneBind  = 0,
    ReadBind  = 1,
    WriteBind = 2,
    ErrorBind = 4,
};

struct Pipe pipe_new();
void pipe_close(struct Pipe* p);

struct String pipe_read(struct Pipe* o);
void pipe_write(struct Pipe* o, struct String data);

struct PipeBind {
    struct Pipe* pipe;
    enum BindType ty;
};

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

struct Pipe* check_pipe(lua_State* L, int idx);
enum BindType lua_check_bind_type(lua_State* L, int idx);

#endif
