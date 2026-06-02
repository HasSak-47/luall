#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include "bindgen_plugin.h"
#include "plugin/definitions.h"

typedef typeof(plugin_setup)* SetupFunction;
typedef typeof(plugin_destruct)* DestructFunction;

struct PluginHandler {
    enum PluginKind kind;
    int setup_table_reference;
    int export_reference;
    int extend_reference;

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

struct PluginHandler prepare_c_plugin(const struct PluginData* plugin);
struct PluginHandler prepare_binary_plugin(const struct PluginData* plugin);
struct PluginHandler prepare_lua_plugin(const struct PluginData* plugin);
struct PluginHandler prepare_rust_plugin(const struct PluginData* plugin);

void unload_c_plugin(lua_State* state, struct PluginHandler* plugin);
void unload_binary_plugin(lua_State* state, struct PluginHandler* unload);
void unload_lua_plugin(lua_State* state, struct PluginHandler* unload);
void unload_rust_plugin(lua_State* state, struct PluginHandler* unload);

#endif
