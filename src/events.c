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

void trigger_hook(struct Event event) {
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == event.kind) {
            if (hd->event.kind == EVENT_USER_DEFINED) {
                // TODO:...
            }
            else {
                for (size_t j = 0; j < hd->hooks.len; ++j) {
                    struct Hook* hook = &hd->hooks.data[j];
                    switch (hook->kind) {
                    case HOOK_KIND_BINARY:
                        lua_pushcfunction(state.L, hd->hooks.data[j].actor);
                        break;
                    case HOOK_KIND_LUA:
                        lua_rawgeti(
                            state.L, LUA_REGISTRYINDEX, hook->reference);
                        break;
                    }
                    lua_pcall(state.L, 1, 0, 0);
                }
            }
            break;
        }
    }
}

void trigger_enter_hook() {
    log_debug("running enter hooks");
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == EVENT_ENTER) {
            log_debug("\trunning hook");
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
                lua_pcall(state.L, 1, 0, 0);
            }
            break;
        }
    }
}

void trigger_exit_hook() {
    log_debug("running exit hooks");
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == EVENT_EXIT) {
            log_debug("running enter hook");
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
                lua_pcall(state.L, 1, 0, 0);
            }
            break;
        }
    }
}

void trigger_input_hook(struct InputKey key) {
    push_input_key(state.L, key);
    for (size_t i = 0; i < state.event.hooks.len; ++i) {
        struct HookData* hd = &state.event.hooks.data[i];
        if (hd->event.kind == EVENT_KEY_INPUT) {
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
                lua_pushvalue(state.L, -2);
                lua_pcall(state.L, 1, 0, 0);
            }
            return;
        }
    }
}
