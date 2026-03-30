#ifndef __PLUGIN_LOADERS_H__
#define __PLUGIN_LOADERS_H__

#include "../state.h"
#include "./definitions.h"

struct PluginHandler load_c_plugin(lua_State* L, struct Plugin* p);

#endif // !__PLUGIN_LOADERS_H__
