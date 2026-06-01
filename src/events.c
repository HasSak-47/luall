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

void on_event_hook(struct Event event, struct String owner, struct Hook hook) {
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        for (size_t j = 0; j < hd->owner.len; ++j) {
            if (string_cmp(hd->owner, owner)) {
                vector_push(hd->hooks, hook);
                break;
            }
        }
    }
}

void on_event(struct Event event, struct String owner, EventActor actor) {
    struct Hook hook = {
        .kind  = HOOK_KIND_BINARY,
        .actor = actor,
    };

    on_event_hook(event, owner, hook);
}

void trigger_enter_hook() {
    log_debug("running enter hooks");
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == EVENT_ENTER) {
            log_debug("running enter hook");
            for (size_t j = 0; j < hd->hooks.len; ++j) {
                struct Hook* hook = &hd->hooks.data[j];
                switch (hook->kind) {
                case HOOK_KIND_BINARY:
                    hd->hooks.data[j].actor(state.L);
                    break;
                case HOOK_KIND_LUA:
                    lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
                    lua_pcall(state.L, 0, 0, 0);
                    break;
                }
                break;
            }
        }
    }
}

void trigger_exit_hook() {
    log_debug("running enter hooks");
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == EVENT_EXIT) {
            log_debug("running enter hook");
            for (size_t j = 0; j < hd->hooks.len; ++j) {
                struct Hook* hook = &hd->hooks.data[j];
                switch (hook->kind) {
                case HOOK_KIND_BINARY:
                    hd->hooks.data[j].actor(state.L);
                    break;
                case HOOK_KIND_LUA:
                    lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
                    lua_pcall(state.L, 0, 0, 0);
                    break;
                }
            }
            break;
        }
    }
}

void trigger_input_hook(struct InputKey key) {
    log_debug("running enter hooks");
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == EVENT_KEY_INPUT) {
            log_debug("running enter hook");
            for (size_t j = 0; j < hd->hooks.len; ++j) {
                struct Hook* hook = &hd->hooks.data[j];
                push_input_key(state.L, key);

                switch (hook->kind) {
                case HOOK_KIND_BINARY:
                    hd->hooks.data[j].actor(state.L);
                    break;
                case HOOK_KIND_LUA:
                    lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
                    lua_pcall(state.L, 0, 0, 0);
                    break;
                }
            }
            break;
        }
    }
}
