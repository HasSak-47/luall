#ifndef __STATE_H__
#define __STATE_H__

#include "vectors.h"
#ifndef CACHE_PATH
#define CACHE_PATH "./.ignore/cache"
#endif

#ifndef CONFIG_PATH
#define CONFIG_PATH "./config/init.lua"
#endif

#include <lua.h>

#include <stdbool.h>

#include <termios.h>

#include "./bindgen_log.h"
#include "./bindgen_plugin.h"
#include "./input_key.h"
#include "./path.h"
#include "./plugin/definitions.h"

// Luall.vars.user
struct User {
    struct String name;
    struct Path home;
};

// Luall.vars.term
struct TerminalState {
    struct termios orig_termios;
    bool got_original;
    bool in_raw_mode;
    bool in_alternate_screen;
};

// Luall.vars
struct Vars {
    struct User user;
    struct Path cwd;
    struct String host;
    int error;

    enum Level log_level;
    struct TerminalState term;
};

struct Config {
    struct Path config;
    struct Path cache;
};

DefineVector(VectorHook, struct Hook);

struct HookData {
    struct Event event;
    struct String owner;
    struct VectorHook hooks;
};

DefineVector(VectorHookData, struct HookData);

struct EventHandler {
    struct VectorHookData hooks;
};

struct ShellState {
    // args passed to the shell
    struct VectorString args;
    struct Vars vars;
    struct Config config;
    struct EventHandler event;

    struct PluginManager* manager;
    bool is_running;
    bool reload;
    lua_State* L;
};

extern struct ShellState state;

bool read_input_key(struct InputKey* key);
void create_input_key_metatable(lua_State* L);
void push_input_key(lua_State* L, struct InputKey key);

void init_shell_state(const char* config_path, const char* cache_path);
void end_shell_state();

void get_current_state();
void update_current_state();

void trigger_enter_hook();
void trigger_exit_hook();
void trigger_input_hook(struct InputKey key);

void unset_raw_mode();
void set_raw_mode();

void enter_alternate_screen();
void leave_alternate_screen();

#endif
