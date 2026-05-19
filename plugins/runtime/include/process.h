#ifndef __PROCESS_H_
#define __PROCESS_H_

#include <lua.h>
#include <stdbool.h>
#include <unistd.h>
#include <vectors.h>

#include <io_api.h>

DefineVector(VectorArgs, char*);

struct Command {
    char* cmd;
    struct VectorArgs args;
    bool foreground;
    pid_t running_pid;

    struct PipeBind pipe;
};

struct Command command_new(const char* path);

void command_reserve_size(struct Command* cmd, size_t size);
void command_bind_pipe(
    struct Command* cmd, struct Pipe* pipe, enum BindType ty);
void command_add_arg(struct Command* cmd, const char* arg);

pid_t command_run(struct Command* p);
int process_wait(pid_t pid);

void process_setup_lua_api(lua_State* state);

#endif
