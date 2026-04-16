#ifndef __STATE_H__
#define __STATE_H__

#include "utils.h"

#ifndef PLUGIN_PATH
#define PLUGIN_PATH "./plugins"
#endif

#ifndef CACHE_PATH
#define CACHE_PATH "./.ignore/cache"
#endif

#ifndef CONFIG_PATH
#define CONFIG_PATH "./config/init.lua"
#endif
#include <lua.h>
#include <stdbool.h>

#include "bindgen.h"
#include "input_key.h"
#include "path.h"
#include "plugin/definitions.h"

typedef typeof(plugin_setup)* SetupFunction;
typedef typeof(plugin_destruct)* DestructFunction;

struct PluginHandler {
    struct Plugin* plugin;
    union {
        struct {
            void* handler;
            SetupFunction setup;
            DestructFunction destruct;
        } c;

        struct {
            int setup_reference;
            int destruct_reference;
        } lua;
    };
};

DefineVector(VectorPluginHandler, struct PluginHandler);

// Luall.vars
struct User {
    struct String name;
    struct Path home;
};

struct Vars {
    struct User user;
    struct Path cwd;
    struct String host;
    int error;
    bool debug;
};

struct Config {
    struct Path config;
    struct Path cache;
    struct Path plugins;
};

struct Hook {
    enum PluginKind kind;
    enum Event event;

    union {
        Actor actor;
        int reference;
    };
};

DefineVector(VectorHook, struct Hook);

struct ShellState {
    // args passed to the shell
    struct VectorString args;
    struct Vars vars;
    struct Config config;
    struct VectorHook hooks;

    struct PluginManager* manager;
    struct VectorPluginHandler plugins;
    bool running;
    bool reload;
    lua_State* L;
};

extern struct ShellState state;

void create_input_key_metatable(lua_State* L);
void push_input_key(lua_State* L, struct InputKey key);

void init_shell_state();
void end_shell_state();

void get_current_state();
void update_current_state();

void trigger_enter_hook();
void trigger_exit_hook();
void trigger_input_hook(struct InputKey key);

#endif
