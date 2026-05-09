/// WARN: header only exists to help bindgen do bindings DO NOT USE
#ifndef __PLUGIN_BINDINGS_H__
#define __PLUGIN_BINDINGS_H__

#include "plugin/definitions.h"

struct PluginData;

enum PluginKind {
    PLUGIN_KIND_LUA    = 0,
    PLUGIN_KIND_BINARY = 1,
    PLUGIN_KIND_C      = 2,
    PLUGIN_KIND_RUST   = 3,
};

typedef typeof(plugin_setup)* SetupFunction;
typedef typeof(plugin_destruct)* DestructFunction;

struct PluginHandler {
    enum PluginKind kind;
    int setup_table_reference;
    union {
        struct {
            void* handler;
            SetupFunction setup;
            DestructFunction destruct;
        };

        struct {
            char* lua_path;
            int setup_reference;
            int destruct_reference;
        };
    };
};

struct PluginHandler load_c_plugin(const struct PluginData* plugin);
struct PluginHandler load_binary_plugin(const struct PluginData* plugin);
struct PluginHandler load_lua_plugin(const struct PluginData* plugin);
struct PluginHandler load_rust_plugin(const struct PluginData* plugin);

void unload_c_plugin(lua_State* state, struct PluginHandler* plugin);
void unload_binary_plugin(lua_State* state, struct PluginHandler* unload);
void unload_lua_plugin(lua_State* state, struct PluginHandler* unload);
void unload_rust_plugin(lua_State* state, struct PluginHandler* unload);

#endif
