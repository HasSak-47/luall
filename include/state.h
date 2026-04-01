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
        } lua;
    };
};

DefineVector(VectorPluginHandler, struct PluginHandler);

// Luall.vars
struct User {
    char* name;
    struct Path home;
};

struct Vars {
    struct User user;
    struct Path cwd;
    char* host;
    int error;
    bool debug;
};

struct Config {
    struct Path config;
    struct Path cache;
    struct Path plugins;
};

struct ShellState {
    struct Vars vars;
    struct Config config;
    struct VectorPluginHandler plugins;
    bool running;
    bool reload;
    lua_State* L;
};

extern struct ShellState state;

void init_shell_state();
void end_shell_state();

void get_current_state();
void update_current_state();

void run_hooks();

#endif
