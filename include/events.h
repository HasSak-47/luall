#ifndef __EVENT_H__
#define __EVENT_H__

#include <lua.h>
#include "ly_string.h"

enum EventKind {
    EVENT_KEY_INPUT,
    EVENT_ENTER,
    EVENT_EXIT,
    EVENT_USER_DEFINED,
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

typedef int (*PushEventArg)(lua_State* L, void* data);

void create_event(struct String name, struct String owner);
void trigger_event(struct Event event, PushEventArg push, void* data);
void on_event(struct Event event, struct String owner, EventActor actor);
void on_event_hook(struct Event event, struct String owner, struct Hook hook);

#endif
