#include <debug.h>
#include <logs.h>
#include <process.h>
#include <state.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <lauxlib.h>
#include <lua.h>
#include "vectors.h"

/**
 * takes an string cmd and clones it
 */
struct Command command_new(const char* cmd) {
    struct Command c = {
        .cmd        = strdup(cmd),
        .args       = {0, 0, 0},
        .foreground = true,
    };
    log_debug("creating cmd for path: %p %s", cmd, cmd);
    // first arg is the name
    command_add_arg(&c, cmd);

    return c;
}

void command_reserve_size(struct Command* cmd, size_t argc) {
    vector_reserve(cmd->args, argc);
}

/**
 * takes an string and and clones it
 */
void command_add_arg(struct Command* cmd, const char* arg) {
    log_debug("add_arg: %p %s", arg, arg);
    char* clone_arg = arg != NULL ? strdup(arg) : NULL;
    vector_push(cmd->args, clone_arg);
}

/**
 * Runs a given command
 *
 * It frees all the command info at exit
 * returns the pid of the child, it does not wait for it to stop
 */
pid_t command_run(struct Command* p) {
    // all commands must end with a trailing NULL
    command_add_arg(p, NULL);
    if (state.vars.log_level >= LEVEL_DEBUG) {
        log_debug("[parent]: running command: %s", p->cmd);
        for (size_t i = 0; i < p->args.len; ++i) {
            log_debug(" %s", p->args.data[i]);
        }
        log_debug("");
    }

    pid_t pid = fork();
    if (pid == 0) { // child
        log_debug("[child]: name: %s", p->cmd);
        if (p->foreground) {
            log_debug("setting to foreground");
            unset_raw_mode();
            // set_to_foreground();
            log_debug("[child]: child is foreground");
        }
        log_debug("[child]: setting io binds");

        log_debug("[child]: running bind %d", p->bind.len);
        for (size_t i = 0; i < p->bind.len; ++i) {
            log_debug("[child] running bind %d", i);
            struct ProcessBind* bind = &p->bind.data[i];
            enum ProcessBindKind k =
                (p->binded_in ? (bind->kind & PROCESS_READ) : 0) |
                (p->binded_out ? (bind->kind & PROCESS_WRITE) : 0) |
                (p->binded_err ? (bind->kind & PROCESS_ERROR) : 0);

            (bind->vt->bind)(bind->handler, k);
        }

        log_debug("[child]: executing cmd %s...", p->cmd);
        int r = execv(p->cmd, p->args.data);
        if (r < 0)
            temporal_suicide_msg("[child] didn't exec :)");
    }
    else if (pid < 0) {
        temporal_suicide_msg("[parent]: didn't fork?");
    }

    // parent
    log_debug("[parent]: cleaning command data");
    for (char** arg = p->args.data; *arg != NULL; ++arg) {
        log_debug("[parent]: removing arg: %p %s", *arg, *arg);
        free(*arg);
    }

    free(p->args.data);
    free(p->cmd);
    log_debug("[parent]: returning pid");
    p->cmd         = NULL;
    p->args.data   = NULL;
    p->running_pid = pid;
    return pid;
}

int process_wait(pid_t pid) {
    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        temporal_suicide_msg("[parent]: waitpid failed");
    }

    // set_to_foreground();
    set_raw_mode();

    return status;
}

void command_bind_io(struct Command* cmd, struct ProcessBind bind) {
    vector_push(cmd->bind, bind);
}

static void file_bind(struct FileHandler* handler, enum ProcessBindKind kind) {
    if (kind & PROCESS_ERROR && handler->mode & OPEN_MODE_WRITE) {
        dup2(handler->fd, STDERR_FILENO);
    }

    if (kind & PROCESS_WRITE && handler->mode & OPEN_MODE_WRITE) {
        dup2(handler->fd, STDOUT_FILENO);
    }

    if (kind & PROCESS_READ && handler->mode & OPEN_MODE_READ) {
        dup2(handler->fd, STDIN_FILENO);
    }
}

static void pipe_bind(struct Pipe* handler, enum ProcessBindKind kind) {
    log_debug("running pipe binding");
    bool out = false;
    bool in  = false;

    if (kind & PROCESS_ERROR) {
        dup2(handler->p[1], STDERR_FILENO);
        out = true;
    }

    if (kind & PROCESS_WRITE) {
        dup2(handler->p[1], STDOUT_FILENO);
        out = true;
    }

    if (kind & PROCESS_READ) {
        dup2(handler->p[0], STDIN_FILENO);
        in = true;
    }

    if (!out) {
        close(handler->p[1]);
    }

    if (!in) {
        close(handler->p[0]);
    }
}

static void lua_bind(struct LuaBind handler, enum ProcessBindKind kind) {
    lua_rawgeti(state.L, LUA_REGISTRYINDEX, handler.reference);
    lua_getfield(state.L, -1, "bind");

    if (!lua_isfunction(state.L, -1)) {
        lua_pop(state.L, 2);
        return;
    }

    lua_pushvalue(state.L, -2);
    lua_pushinteger(state.L, kind);
    if (lua_pcall(state.L, 2, 0, 0) != LUA_OK) {
        lua_pop(state.L, 1);
    }
}

static void lua_bind_delete(struct LuaBind handler) {
    luaL_unref(state.L, LUA_REGISTRYINDEX, handler.reference);
}

static void noop(void* data) {}

const struct BindProcessVTable vtable_file = {
    (FnBindProcessBind)file_bind, (FnBindProcessDelete)noop};

const struct BindProcessVTable vtable_pipe = {
    (FnBindProcessBind)pipe_bind, (FnBindProcessDelete)noop};
const struct BindProcessVTable vtable_lua = {
    (FnBindProcessBind)lua_bind, (FnBindProcessDelete)lua_bind_delete};

struct ProcessBind bind_from_pipe(struct Pipe* pipe) {
    return (struct ProcessBind){
        (void*)pipe,
        &vtable_pipe,
        PROCESS_NONE,
    };
}

struct ProcessBind bind_from_file(struct FileHandler* file) {
    return (struct ProcessBind){
        (void*)file,
        &vtable_file,
        PROCESS_NONE,
    };
};

struct ProcessBind bind_from_lua(struct LuaBind lua) {
    void* handler = malloc(sizeof(struct LuaBind));
    memcpy(handler, &lua, sizeof(struct LuaBind));
    return (struct ProcessBind){
        (void*)handler,
        &vtable_lua,
        PROCESS_NONE,
    };
};
