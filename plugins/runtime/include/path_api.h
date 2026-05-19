#ifndef __PATH_API_H__
#define __PATH_API_H__

#include <lua.h>
#define LUA_PATH_MT "lyra.api.path"

void path_setup_lua_api(lua_State* L);
struct Path* check_path(lua_State* L, int idx);

#endif
