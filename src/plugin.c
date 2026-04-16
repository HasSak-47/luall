#include <dirent.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>

#include <debug.h>
#include <ly_string.h>
#include <path.h>
#include <state.h>
#include <utils.h>
#include "bindgen.h"

void get_units(struct VectorPath* v, struct Path curr_dir) {
    debug_printf("reading units:...\n");
    if (!path_is_dir(&curr_dir)) {
        debug_printf("found leaf\n");
        return;
    }

    struct VectorPath childs = path_get_childs(&curr_dir);

    for (size_t i = 0; i < childs.len; ++i) {
        struct Path* p     = &childs.data[i];
        struct String name = path_get_name(p);
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

// WARNING: THIS IS FULL OF MEMORY LEAKS FIX IN THE FUTURE!!
struct PluginHandler load_c_plugin(lua_State* L, struct Plugin* p) {
    struct PluginHandler handler = {};
    handler.plugin               = p;
    char* path_str               = plugin_get_path(handler.plugin);
    char* name_str               = plugin_get_name(handler.plugin);

    debug_printf("loading c plugin %s @ %s\n", name_str, path_str);

    struct Path path         = path_parse(path_str);
    struct Path include_path = path_clone(&path);
    path_push_name(&include_path, "include");
    free(path_str);

    struct VectorPath compilation_units = {0, 0, 0};
    get_units(&compilation_units, path);

    debug_printf("compilation_units (%lu) @ %p\n", compilation_units.len,
        compilation_units.data);

    struct VectorString args = {};
    debug_printf("creating argv...\n");
    debug_printf("pushing compiler name\n");
    vector_push(args, string_from_cstr("/bin/gcc"));

    debug_printf("pushing compiling flags\n");
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; ++i) {
        vector_push(args, string_from_cstr(flags[i]));
    }
    char* include_path_str = path_get_string(include_path);
    vector_push(args, string_from_cstr("-I"));
    vector_push(args, string_from_cstr(include_path_str));

    debug_printf("pushing units\n");
    for (size_t i = 0; i < compilation_units.len; ++i) {
        char* path_str = path_get_string(compilation_units.data[i]);
        debug_printf("\tpushing unit: %s @ %p\n", path_str, path_str);
        vector_push(args, string_from_cstr(path_str));
        debug_printf("\tcleaning temp var...\n");

        free(path_str);
    }

    debug_printf("generating cache location\n");
    struct Path cache_path = path_clone(&state.config.cache);
    path_push_name(&cache_path, name_str);
    char* plugin_cache_path = path_get_string(cache_path);

    debug_printf("generating compiler output path\n");
    vector_push(args, string_from_cstr("-o"));
    vector_push(args, string_from_cstr(plugin_cache_path));

    debug_printf("generating argv\n");
    char** argv = malloc(sizeof(char*) * (args.len + 1));

    debug_printf("copying from String to cstr\n");
    for (size_t i = 0; i < args.len; ++i) {
        argv[i] = string_to_cstring(args.data[i]);
        debug_printf("added: %s\n", argv[i]);
    }
    argv[args.len] = NULL;
    pid_t pid      = fork();
    if (pid == 0) {
        execv("/bin/gcc", argv);
        printf("execv failed to run");
        exit(-1);
    }
    else if (pid > 0) {
        debug_printf("pid %lu\n", pid);
        int status = 0;
        waitpid(pid, &status, 0);
        bool compiled = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        debug_printf("compilation_status %s\n", compiled ? "ok" : "err");
        if (!compiled) {
            printf("plugin compilation failed, refusing to run\n");
            return handler;
        }
    }

    path_destruct(&path);
    handler.c.handler  = dlopen(argv[args.len - 1], RTLD_LAZY);
    if (!handler.c.handler) {
        printf("failed to load plugin shared object: %s\n", dlerror());
        return handler;
    }
    handler.c.setup    = dlsym(handler.c.handler, "plugin_setup");
    handler.c.destruct = dlsym(handler.c.handler, "plugin_destruct");

    return handler;
}
