#ifndef __STATE_H__
#define __STATE_H__

#include <signal.h>
#include <sys/ioctl.h>

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
#include "./events.h"
#include "./input_key.h"
#include "./path.h"
#include "./vectors.h"

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

    struct winsize window_size;
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
    // the event
    struct Event event;
    // which plugin owns the event
    struct String owner;
    // which plugin the hooks
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
    struct sigaction sa;

    struct PluginManager* manager;
    bool is_running;
    bool reload;
    lua_State* L;
};

extern struct ShellState state;

// input stuff
bool read_input_key(struct InputKey* key);
void create_input_key_metatable(lua_State* L);
int push_input_key(lua_State* L, struct InputKey* key);

// signal stuff
void create_signal_metatable(lua_State* L);
int push_signal(lua_State* L, enum Signal* signal);

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

int register_plugin_module_paths(lua_State* L, const char* plugin_path);

#endif
