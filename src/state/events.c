#include "events.h"
#include <lauxlib.h>
#include <lua.h>

#include <logs.h>
#include <ly_string.h>
#include <path.h>
#include <plugin.h>
#include <plugin/definitions.h>
#include <state.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>
#include "vectors.h"

void on_event_hook(struct Event event, struct String owner, struct Hook hook) {
    log_debug("registering hook %d...", event.kind);
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == event.kind) {
            if (hd->event.kind == EVENT_USER_DEFINED &&
                string_cmp(hd->event.name, event.name)) {
                // TODO:...
                log_warn("custom events not implemented yet");
            }
            else {
                vector_push(hd->hooks, hook);
            }
            return;
        }
    }
    log_debug("failed to register hook...");
}

void on_event(struct Event event, struct String owner, EventActor actor) {
    struct Hook hook = {
        .kind  = HOOK_KIND_BINARY,
        .actor = actor,
    };

    on_event_hook(event, owner, hook);
}

void trigger_event(struct Event event, PushEventArg push, void* data) {
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == event.kind) {
            if (event.kind == EVENT_USER_DEFINED) {
                // TODO: ...
                break;
            }
            for (size_t j = 0; j < hd->hooks.len; ++j) {
                struct Hook* hook = &hd->hooks.data[j];
                switch (hook->kind) {
                case HOOK_KIND_BINARY:
                    lua_pushcfunction(state.L, hd->hooks.data[j].actor);
                    break;
                case HOOK_KIND_LUA:
                    lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
                    break;
                }
                size_t arg_len = push(state.L, data);
                lua_pcall(state.L, arg_len, 0, 0);
            }
        }
    }
}

static int push_empty(lua_State* _state, void* _void) {
    return 0;
}

void trigger_enter_hook() {
    log_debug("running enter hooks");
    trigger_event(
        (struct Event){
            EVENT_ENTER,
            {},
        },
        (PushEventArg)push_empty, NULL);
}

void trigger_exit_hook() {
    log_debug("running exit hooks");
    trigger_event(
        (struct Event){
            EVENT_EXIT,
            {},
        },
        (PushEventArg)push_empty, NULL);
}

void trigger_input_hook(struct InputKey key) {
    trigger_event(
        (struct Event){
            EVENT_KEY_INPUT,
            {},
        },
        (PushEventArg)push_input_key, &key);
}

void create_event(struct String name, struct String owner) {
    struct HookData hook = ((struct HookData){
        .event = (struct Event){.kind = EVENT_ENTER, .name = name},
        .owner = owner,
        .hooks = {},
    });

    vector_push(state.event.hooks, hook);
}

struct Event hook_input() {
    return (struct Event){.kind = EVENT_KEY_INPUT, .name = {}};
}
struct Event hook_enter() {
    return (struct Event){.kind = EVENT_ENTER, .name = {}};
}
struct Event hook_exit() {
    return (struct Event){.kind = EVENT_EXIT, .name = {}};
}
struct Event hook_signal() {
    return (struct Event){.kind = EVENT_SIGNAL, .name = {}};
}
struct Event hook_user(struct String name) {
    return (struct Event){.kind = EVENT_USER_DEFINED, .name = name};
}

void signal_handler(int code) {}
