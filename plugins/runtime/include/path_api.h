#ifndef __STRING_API_H__
#define __STRING_API_H__

#include <lua.h>
#define LUA_PATH_MT "lyra.api.path"

void path_setup_lua_api(lua_State* L);

#endif
