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

void add_hook(enum Event event, Actor actor) {
    struct Hook hook = {
        .kind  = PLUGIN_KIND_C,
        .event = event,
        .actor = actor,
    };

    vector_push(state.hooks, hook);
}

static void run_event_enter_hook(struct Hook* hook) {
    log_debug("running enter event");
    if (hook->kind == PLUGIN_KIND_C) {
        hook->actor(state.L);
    }
    else {
        lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
        lua_pcall(state.L, 0, 0, 0);
    }
}

static void run_event_exit_hook(struct Hook* hook) {
    log_debug("running exit event");
    if (hook->kind == PLUGIN_KIND_C) {
        hook->actor(state.L);
    }
    else {
        lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
        lua_pcall(state.L, 0, 0, 0);
    }
}

static void run_event_key_input_hook(struct InputKey key, struct Hook* hook) {
    log_trace("running input event");
    if (hook->kind == PLUGIN_KIND_C) {
        push_input_key(state.L, key);
        hook->actor(state.L);
    }
    else {
        lua_rawgeti(state.L, LUA_REGISTRYINDEX, hook->reference);
        push_input_key(state.L, key);
        lua_pcall(state.L, 1, 0, 0);
    }
}

void trigger_enter_hook() {
    log_debug("running enter hooks");
    for (size_t i = 0; i < state.hooks.len; ++i) {
        log_debug("running hook #%lu", i);
        if (state.hooks.data[i].event == EVENT_ENTER) {
            run_event_enter_hook(&state.hooks.data[i]);
        }
    }
}

void trigger_exit_hook() {
    log_debug("running exit hook...");
    for (size_t i = 0; i < state.hooks.len; ++i) {
        if (state.hooks.data[i].event == EVENT_EXIT) {
            run_event_exit_hook(&state.hooks.data[i]);
            ;
        }
    }
}

void trigger_input_hook(struct InputKey key) {
    for (size_t i = 0; i < state.hooks.len; ++i) {
        if (state.hooks.data[i].event == EVENT_KEY_INPUT) {
            run_event_key_input_hook(key, &state.hooks.data[i]);
            ;
        }
    }
}
