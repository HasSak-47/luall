#include <plugin/definitions.h>
#include <process.h>
#include <vars.h>
#include "path_api.h"

int plugin_setup(lua_State* L) {
    process_setup_lua_api(L);
    path_setup_lua_api(L);
    // vars_setup_lua_api(L);

    return 0;
}

int plugin_destruct(lua_State* L) {
    return 0;
}
