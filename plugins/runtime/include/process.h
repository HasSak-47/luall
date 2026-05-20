#ifndef __PROCESS_H_
#define __PROCESS_H_

#include <lua.h>
#include <stdbool.h>
#include <unistd.h>
#include <vectors.h>

#include <io_api.h>

DefineVector(VectorArgs, char*);

enum ProcessBindKind {
    PROCESS_NONE  = 0,
    PROCESS_READ  = 1,
    PROCESS_WRITE = 2,
    PROCESS_ERROR = 4,
};

typedef void (*FnBindProcessBind)(void* data, enum ProcessBindKind kind);
typedef void (*FnBindProcessDelete)(void* data);

struct BindProcessVTable {
    FnBindProcessBind bind;
    FnBindProcessDelete delete;
};

struct LuaBind {
    int reference;
};

struct ProcessBind {
    void* handler;
    const struct BindProcessVTable* vt;

    enum ProcessBindKind kind;
};

DefineVector(VectorProcessBind, struct ProcessBind);

struct Command {
    char* cmd;
    struct VectorArgs args;
    bool foreground;
    pid_t running_pid;

    bool binded_in;
    bool binded_out;
    bool binded_err;
    struct VectorProcessBind bind;
};

struct Command command_new(const char* path);

void command_reserve_size(struct Command* cmd, size_t size);
void command_bind_io(struct Command* cmd, struct ProcessBind bind);
void command_add_arg(struct Command* cmd, const char* arg);

pid_t command_run(struct Command* p);
int process_wait(pid_t pid);

void process_setup_lua_api(lua_State* state);

// non owning
struct ProcessBind bind_from_pipe(struct Pipe* pipe);
// non owning
struct ProcessBind bind_from_file(struct FileHandler* file);
// owning
struct ProcessBind bind_from_lua(struct LuaBind bind);
#endif
