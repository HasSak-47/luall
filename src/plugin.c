#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// maybe insert a temporal makefile so it can use artifacts?
// WARNING: THIS IS FULL OF MEMORY LEAKS FIX IN THE FUTURE!!
static void complile_c_plugin_no_makefile(
    struct Path path, const char* cache_str, const char* name_str) {

    struct Path include_path = path_clone(&path);
    path_push_name(&include_path, "include");

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

    debug_printf("generating compiler output path\n");
    vector_push(args, string_from_cstr("-o"));
    vector_push(args, string_from_cstr(cache_str));

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
        }
    }

    path_destruct(&path);

    char* bin_path = malloc(strlen(argv[args.len - 1]));
    strcpy(bin_path, argv[args.len - 1]);
    free(argv);
}

// WARN: MEMORY LEAK YET I DO NOT CARE
static void complile_c_plugin_makefile(struct Path path, const char* object_str,
    const char* artifact_str, const char* name_str) {

    struct VectorString args = {};
    debug_printf("creating argv...\n");
    debug_printf("pushing make\n");
    vector_push(args, string_from_cstr("/bin/make"));
    vector_push(args, string_from_cstr("-C"));
    vector_push(args, string_from_cstr(path_get_string(path)));

    vector_push(args, string_from_cstr("CFLAGS+= '-I../../include'"));

    debug_printf("pushing artifact flags\n");

    struct String out_flag = string_from_cstr("OUT");
    string_concat_cstr(&out_flag, "=");
    string_concat_cstr(&out_flag, object_str);

    vector_push(args, out_flag);

    struct String compilie_flag = string_from_cstr("OBJ_DIR");
    string_concat_cstr(&compilie_flag, "=");
    string_concat_cstr(&compilie_flag, artifact_str);

    vector_push(args, compilie_flag);

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
        execv("/bin/make", argv);
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
        }
    }

    path_destruct(&path);

    char* bin_path = malloc(strlen(argv[args.len - 1]));
    strcpy(bin_path, argv[args.len - 1]);
    free(argv);
}

static void complile_c_plugin(const struct PluginData* data) {
    char* path_str     = plugin_get_data_path(data);
    char* object_str   = plugin_get_shared_object_path(data);
    char* artifact_str = plugin_get_compilation_path(data);
    char* name_str     = plugin_get_name(data);

    debug_printf("loading c plugin %s @ %s\n", name_str, path_str);
    struct Path artifact_path = path_parse(artifact_str);
    path_mkdir_p(&artifact_path);

    struct Path path         = path_parse(path_str);
    struct VectorPath childs = path_get_childs(&path);

    bool has_make = false;

    debug_printf("searching for makefile in %lu paths\n", childs.len);
    for (size_t i = 0; i < childs.len; ++i) {
        struct String name = path_get_name(&childs.data[i]);
        debug_printf("\tfile: '%.*s': %lu\n", name.len, name.data, name.len);
        for (size_t j = 0; j < name.len; ++j) {
            name.data[j] = tolower(name.data[j]);
        }

        name.len = 8;
        if (string_cmp_cstring(name, "makefile")) {
            has_make = true;
        }

        free(name.data);
    }
    for (size_t i = 0; i < childs.len; ++i) {
        path_destruct(&childs.data[i]);
    }
    free(childs.data);

    if (has_make) {
        complile_c_plugin_makefile(path, object_str, artifact_str, name_str);
    }
    else {
        complile_c_plugin_no_makefile(path, object_str, name_str);
    }

    free(object_str);
    free(name_str);
}

static struct PluginHandler _load_binary_plugin(const char* path) {
    struct PluginHandler handler = {};
    handler.handler              = dlopen(path, RTLD_LAZY);
    handler.setup                = dlsym(handler.handler, "plugin_setup");
    handler.destruct             = dlsym(handler.handler, "plugin_destruct");

    return handler;
}

struct PluginHandler load_binary_plugin(const struct PluginData* p) {
    struct PluginHandler c =
        _load_binary_plugin(plugin_get_shared_object_path(p));
    c.kind = PLUGIN_KIND_BINARY;
    return c;
}

struct PluginHandler load_c_plugin(const struct PluginData* p) {
    complile_c_plugin(p);
    struct PluginHandler c =
        _load_binary_plugin(plugin_get_shared_object_path(p));
    c.kind = PLUGIN_KIND_C;

    return c;
}

struct PluginHandler load_lua_plugin(const struct PluginData* p) {
    return (struct PluginHandler){
        .kind = PLUGIN_KIND_LUA, .lua_path = plugin_get_data_path(p)};
}

void unload_binary_plugin(lua_State* state, struct PluginHandler* p) {
    dlclose(p->handler);
}

void unload_c_plugin(lua_State* state, struct PluginHandler* p) {
    dlclose(p->handler);
}

void unload_lua_plugin(lua_State* state, struct PluginHandler* p) {
    // TODO: call destructor or something...
}
