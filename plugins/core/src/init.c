#include <plugin/definitions.h>
#include <process.h>

int plugin_setup(lua_State* L) {
    process_setup_lua_api(L);
    return 0;
}

int parse_destruct(lua_State* L) {
    return 0;
}
