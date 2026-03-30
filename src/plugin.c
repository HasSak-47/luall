#include <debug.h>
#include <dirent.h>
#include <ly_string.h>
#include <path.h>
#include <state.h>
#include <stdio.h>
#include "utils.h"

void get_units(struct VectorPath* v, struct Path curr_dir) {
    debug_printf("reading units:...\n");
    if (!path_is_dir(&curr_dir)) {
        debug_printf("found leaf\n");
        return;
    }

    struct VectorPath childs = get_childs(&curr_dir);

    for (size_t i = 0; i < childs.len; ++i) {
        struct Path* p     = &childs.data[i];
        struct String name = get_name(p);
        // TODO: generate a get_file_ext function
        if (name.data[name.len - 2] == '.' && name.data[name.len - 1] == 'c') {
            vector_push(*v, *p);
            continue;
        }

        if (path_is_dir(p)) {
            get_units(v, *p);
        }
    }
}

// TODO: make into a macro
const char* flags[] = {
    "-Iinclude/", "-Wall", "-I/usr/include/lua5.4", "-g", "-fpic", "-shared"};

struct PluginHandler load_c_plugin(lua_State* L, struct Plugin* p) {
    struct PluginHandler handler = {};
    handler.plugin               = p;
    char* path_str               = plugin_get_path(handler.plugin);
    char* plugin_name =
        plugin_get_kind debug_printf("loading c plugin @ %s\n", path_str);

    struct Path path = parse_path(path_str);
    free(path_str);

    struct VectorPath compilation_units = {0, 0, 0};
    get_units(&compilation_units, path);

    debug_printf("compilation_units (%lu) @ %p:\n", compilation_units.len,
        compilation_units.data);
    for (size_t i = 0; i < compilation_units.len; ++i) {
        char* path_str = get_path_string(compilation_units.data[i]);
        debug_printf("\t%s\n", path_str);
        free(path_str);
    }

    destruct_path(&path);

    return handler;
}
