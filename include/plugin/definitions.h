#ifndef __PLUGIN_DEFINITIONS__
#define __PLUGIN_DEFINITIONS__

#include <lua.h>
#include "../events.h" // IWYU pragma: keep

#ifdef __cplusplus
extern "C" {
#endif

int plugin_setup(lua_State* L);
int plugin_destruct(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
