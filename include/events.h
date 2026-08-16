#ifndef __EVENT_H__
#define __EVENT_H__

#include <lua.h>
#include <signal.h>
#include "ly_string.h"

enum EventKind {
    EVENT_KEY_INPUT,
    EVENT_ENTER,
    EVENT_EXIT,
    EVENT_SIGNAL,
    EVENT_USER_DEFINED,
};

enum Signal {
    SIGNAL_INT           = SIGINT,
    SIGNAL_TERM          = SIGTERM,
    SIGNAL_CONT          = SIGCONT,
    SIGNAL_QUIT          = SIGQUIT,
    SIGNAL_HUP           = SIGHUP,
    SIGNAL_CHLD          = SIGCHLD,
    SIGNAL_ALRM          = SIGALRM,
    SIGNAL_PIPE          = SIGPIPE,
    SIGNAL_SEGV          = SIGSEGV,
    SIGNAL_FPE           = SIGFPE,
    SIGNAL_ABRT          = SIGABRT,
    SIGNAL_USR1          = SIGUSR1,
    SIGNAL_USR2          = SIGUSR2,
    SIGNAL_WINDOW_CHANGE = SIGWINCH,
};

struct Event {
    enum EventKind kind;
    struct String name;
};

enum HookKind {
    HOOK_KIND_LUA,
    HOOK_KIND_BINARY,
};

typedef int (*EventActor)(lua_State* L);

struct Hook {
    struct String owner;
    enum HookKind kind;
    union {
        EventActor actor;
        int reference;
    };
};

struct Event hook_input();
struct Event hook_enter();
struct Event hook_exit();
struct Event hook_signal();
struct Event hook_user(struct String name);

typedef int (*PushEventArg)(lua_State* L, void* data);

void create_event(struct String name, struct String owner);
void trigger_event(struct Event event, PushEventArg push, void* data);
void on_event(struct Event event, struct String owner, EventActor actor);
void on_event_hook(struct Event event, struct String owner, struct Hook hook);

#endif
