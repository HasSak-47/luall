#ifndef __PLUGIN_DEFINITIONS__
#define __PLUGIN_DEFINITIONS__

#include <lua.h>
#include "../ly_string.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    enum HookKind kind;
    union {
        EventActor actor;
        int reference;
    };
};

int plugin_setup(lua_State* L);
int plugin_destruct(lua_State* L);

void create_event(struct String name, struct String owner);
void trigger_event(struct Event event);
void on_event(struct Event event, struct String owner, EventActor actor);
void on_event_hook(struct Event event, struct String owner, struct Hook hook);

#ifdef __cplusplus
}
#endif

#endif
