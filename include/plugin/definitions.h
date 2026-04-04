#ifndef __PLUGIN_DEFINITIONS__
#define __PLUGIN_DEFINITIONS__

#include <lua.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Event {
    EVENT_KEY_INPUT,
    EVENT_ENTER,
    EVENT_EXIT,
};

int plugin_setup(lua_State* L);
int plugin_destruct(lua_State* L);

typedef int (*Actor)(lua_State* L);
void add_hook(enum Event event, Actor);

#ifdef __cplusplus
}
#endif

#endif
