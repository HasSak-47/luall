#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>

#include <logs.h>
#include <ly_string.h>
#include <path.h>
#include <plugin.h>
#include <state.h>

static void free_argv(char** argv) {
    if (argv == NULL) {
        return;
    }

    for (size_t i = 0; argv[i] != NULL; ++i) {
        free(argv[i]);
    }
    free(argv);
}

static void vector_string_destruct(struct VectorString* strings) {
    for (size_t i = 0; i < strings->len; ++i) {
        free(strings->data[i].data);
    }

    free(strings->data);
    strings->data = NULL;
    strings->len  = 0;
    strings->cap  = 0;
}

static void vector_path_destruct(struct VectorPath* paths) {
    for (size_t i = 0; i < paths->len; ++i) {
        path_destruct(&paths->data[i]);
    }

    free(paths->data);
    paths->data = NULL;
    paths->len  = 0;
    paths->cap  = 0;
}

void get_units(struct VectorPath* v, struct Path curr_dir) {
    log_trace("reading units:...");
    if (!path_is_dir(&curr_dir)) {
        log_trace("found leaf");
        return;
    }

    struct VectorPath childs = path_get_childs(&curr_dir);

    for (size_t i = 0; i < childs.len; ++i) {
        struct Path* p     = &childs.data[i];
        struct String name = path_get_name(p);
        // TODO: generate a get_file_ext function
        if (name.len >= 2 && name.data[name.len - 2] == '.' &&
            name.data[name.len - 1] == 'c') {
            vector_push(*v, *p);
            *p = (struct Path){0};
            free(name.data);
            continue;
        }

        if (path_is_dir(p)) {
            get_units(v, *p);
        }

        free(name.data);
        path_destruct(p);
    }

    vector_path_destruct(&childs);
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

    log_debug("compilation_units (%lu) @ %p", compilation_units.len,
        compilation_units.data);

    struct VectorString args = {};
    log_debug("creating argv...");
    vector_push(args, string_from_cstr("/bin/gcc"));

    log_trace("pushing compiling flags");
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; ++i) {
        vector_push(args, string_from_cstr(flags[i]));
    }
    char* include_path_str = path_get_string(include_path);
    vector_push(args, string_from_cstr("-I"));
    vector_push(args, string_from_cstr(include_path_str));
    free(include_path_str);

    log_trace("pushing units");
    for (size_t i = 0; i < compilation_units.len; ++i) {
        char* path_str = path_get_string(compilation_units.data[i]);
        log_trace("\tpushing unit: %s @ %p", path_str, path_str);
        vector_push(args, string_from_cstr(path_str));

        free(path_str);
    }

    log_trace("generating compiler output path");
    vector_push(args, string_from_cstr("-o"));
    vector_push(args, string_from_cstr(cache_str));

    log_trace("generating argv");
    char** argv = malloc(sizeof(char*) * (args.len + 1));

    log_trace("copying from String to cstr");
    for (size_t i = 0; i < args.len; ++i) {
        argv[i] = string_to_cstring(args.data[i]);
        log_trace("added: %s", argv[i]);
    }
    argv[args.len] = NULL;
    pid_t pid      = fork();
    if (pid == 0) {
        execv("/bin/gcc", argv);
        printf("execv failed to run");
        exit(-1);
    }
    else if (pid > 0) {
        log_debug("pid %lu", pid);
        int status = 0;
        waitpid(pid, &status, 0);
        bool compiled = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        log_debug("compilation_status %s", compiled ? "ok" : "err");
        if (!compiled) {
            printf("plugin compilation failed, refusing to run\n");
        }
    }

    path_destruct(&path);
    path_destruct(&include_path);
    vector_path_destruct(&compilation_units);
    free_argv(argv);
    vector_string_destruct(&args);
}

// WARN: MEMORY LEAK YET I DO NOT CARE
static void complile_c_plugin_makefile(struct Path path, const char* object_str,
    const char* artifact_str, const char* name_str) {

    struct VectorString args = {};
    log_debug("creating argv...");
    log_debug("pushing make");
    vector_push(args, string_from_cstr("/bin/make"));
    vector_push(args, string_from_cstr("-C"));
    char* path_str = path_get_string(path);
    vector_push(args, string_from_cstr(path_str));
    free(path_str);

    vector_push(args, string_from_cstr("CFLAGS+= '-I../../include'"));

    log_debug("pushing artifact flags");

    struct String out_flag = string_from_cstr("OUT");
    string_concat_cstr(&out_flag, "=");
    string_concat_cstr(&out_flag, object_str);

    vector_push(args, out_flag);

    struct String compilie_flag = string_from_cstr("OBJ_DIR");
    string_concat_cstr(&compilie_flag, "=");
    string_concat_cstr(&compilie_flag, artifact_str);

    vector_push(args, compilie_flag);

    log_debug("generating argv");
    char** argv = malloc(sizeof(char*) * (args.len + 1));

    log_debug("copying from String to cstr");
    for (size_t i = 0; i < args.len; ++i) {
        argv[i] = string_to_cstring(args.data[i]);
        log_trace("added: %s", argv[i]);
    }
    argv[args.len] = NULL;
    pid_t pid      = fork();
    if (pid == 0) {
        execv("/bin/make", argv);
        printf("execv failed to run");
        exit(-1);
    }
    else if (pid > 0) {
        log_debug("pid %lu", pid);
        int status = 0;
        waitpid(pid, &status, 0);
        bool compiled = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        log_debug("compilation_status %s", compiled ? "ok" : "err");
        if (!compiled) {
            printf("plugin compilation failed, refusing to run\n");
        }
    }

    path_destruct(&path);
    free_argv(argv);
    vector_string_destruct(&args);
}

static void complile_c_plugin(const struct PluginData* data) {
    char* path_str     = plugin_get_data_path(data);
    char* object_str   = plugin_get_shared_object_path(data);
    char* artifact_str = plugin_get_compilation_path(data);
    char* name_str     = plugin_get_name(data);

    log_debug("loading c plugin %s @ %s", name_str, path_str);
    struct Path artifact_path = path_parse(artifact_str);
    path_mkdir_p(&artifact_path);

    struct Path path         = path_parse(path_str);
    struct VectorPath childs = path_get_childs(&path);

    bool has_make = false;

    log_debug("searching for makefile in %lu paths", childs.len);
    for (size_t i = 0; i < childs.len; ++i) {
        struct String name = path_get_name(&childs.data[i]);
        log_trace("\tfile: '%.*s': %lu", name.len, name.data, name.len);
        for (size_t j = 0; j < name.len; ++j) {
            name.data[j] = tolower(name.data[j]);
        }

        if (string_cmp_cstring(name, "makefile")) {
            has_make = true;
        }

        free(name.data);
    }
    vector_path_destruct(&childs);

    if (has_make) {
        complile_c_plugin_makefile(path, object_str, artifact_str, name_str);
    }
    else {
        complile_c_plugin_no_makefile(path, object_str, name_str);
    }

    free(object_str);
    free(path_str);
    free(artifact_str);
    free(name_str);
    path_destruct(&artifact_path);
}

static struct PluginHandler _prepare_binary_plugin(const char* path) {
    log_debug("loading binary at: %s", path);
    struct PluginHandler handler = {};
    handler.handler              = dlopen(path, RTLD_LAZY);
    handler.setup                = dlsym(handler.handler, "plugin_setup");
    handler.destruct             = dlsym(handler.handler, "plugin_destruct");
    log_debug(
        "symbols %p %p %p", handler.handler, handler.setup, handler.destruct);

    return handler;
}

struct PluginHandler prepare_binary_plugin(const struct PluginData* p) {
    char* object_str       = plugin_get_shared_object_path(p);
    struct PluginHandler c = _prepare_binary_plugin(object_str);
    c.kind                 = PLUGIN_KIND_BINARY;
    free(object_str);
    return c;
}

struct PluginHandler prepare_c_plugin(const struct PluginData* p) {
    complile_c_plugin(p);
    char* object_str       = plugin_get_shared_object_path(p);
    struct PluginHandler c = _prepare_binary_plugin(object_str);
    c.kind                 = PLUGIN_KIND_C;
    free(object_str);

    return c;
}

struct PluginHandler prepare_rust_plugin(const struct PluginData* p) {
    char* path_str     = plugin_get_data_path(p);
    char* object_str   = plugin_get_shared_object_path(p);
    char* artifact_str = plugin_get_compilation_path(p);
    char* name_str     = plugin_get_name(p);

    struct Path artifact_path = path_parse(artifact_str);
    path_mkdir_p(&artifact_path);

    struct Path manifest_path = path_parse(path_str);
    path_push_name(&manifest_path, "Cargo.toml");
    char* manifest_str = path_get_string(manifest_path);

    struct Path target_path = path_parse(artifact_str);
    path_push_name(&target_path, "cargo");
    char* target_str = path_get_string(target_path);

    char* cargo_argv[] = {
        "cargo",
        "build",
        "--manifest-path",
        manifest_str,
        "--target-dir",
        target_str,
        NULL,
    };

    log_debug("building rust plugin %s @ %s", name_str, path_str);
    bool compiled = false;

    pid_t pid = fork();
    if (pid == 0) {
        execvp(cargo_argv[0], cargo_argv);
        printf("execvp failed to run %s\n", cargo_argv[0]);
        exit(-1);
    }
    else if (pid > 0) {
        log_debug("pid %lu", pid);
        int status = 0;
        waitpid(pid, &status, 0);
        compiled = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    log_debug("rust plugin compilation_status %s", compiled ? "ok" : "err");

    struct PluginHandler handler = {};
    if (compiled) {
        struct Path built_object_path = path_parse(target_str);
        path_push_name(&built_object_path, "debug");

        struct String lib_name = string_from_cstr("lib");
        string_concat_cstr(&lib_name, name_str);
        string_concat_cstr(&lib_name, ".so");
        path_push_name_string(&built_object_path, lib_name);

        char* built_object_str = path_get_string(built_object_path);
        bool copied            = false;

        char* copy_argv[] = {
            "/bin/cp",
            built_object_str,
            object_str,
            NULL,
        };

        pid = fork();
        if (pid == 0) {
            execv("/bin/cp", copy_argv);
            printf("execv failed to run /bin/cp\n");
            exit(-1);
        }
        else if (pid > 0) {
            log_debug("pid %lu", pid);
            int status = 0;
            waitpid(pid, &status, 0);
            copied = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        }

        if (copied) {
            handler      = _prepare_binary_plugin(object_str);
            handler.kind = PLUGIN_KIND_RUST;
        }
        else {
            printf("rust plugin artifact copy failed, refusing to run\n");
        }

        free(built_object_str);
        path_destruct(&built_object_path);
    }
    else {
        printf("rust plugin compilation failed, refusing to run\n");
    }

    free(path_str);
    free(object_str);
    free(artifact_str);
    free(name_str);
    free(manifest_str);
    free(target_str);
    path_destruct(&artifact_path);
    path_destruct(&manifest_path);
    path_destruct(&target_path);

    return handler;
}

struct PluginHandler prepare_lua_plugin(const struct PluginData* p) {
    return (struct PluginHandler){
        .kind = PLUGIN_KIND_LUA, .lua_path = plugin_get_data_path(p)};
}

void unload_binary_plugin(lua_State* state, struct PluginHandler* p) {
    dlclose(p->handler);
}

void unload_c_plugin(lua_State* state, struct PluginHandler* p) {
    dlclose(p->handler);
}

void unload_rust_plugin(lua_State* state, struct PluginHandler* p) {
    dlclose(p->handler);
}

void unload_lua_plugin(lua_State* state, struct PluginHandler* p) {
    if (p->destruct_reference != 0) {
        lua_rawgeti(state, LUA_REGISTRYINDEX, p->destruct_reference);
        if (lua_pcall(state, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(state, -1);
            log_error("lua plugin unload failed: %s", err);
            lua_pop(state, 1);
        }
        luaL_unref(state, LUA_REGISTRYINDEX, p->destruct_reference);
        p->destruct_reference = 0;
    }

    if (p->setup_reference != 0) {
        luaL_unref(state, LUA_REGISTRYINDEX, p->setup_reference);
        p->setup_reference = 0;
    }

    if (p->export_reference != 0) {
        luaL_unref(state, LUA_REGISTRYINDEX, p->export_reference);
        p->export_reference = 0;
    }

    if (p->extend_reference != 0) {
        luaL_unref(state, LUA_REGISTRYINDEX, p->extend_reference);
        p->extend_reference = 0;
    }

    free(p->lua_path);
    p->lua_path = NULL;
}
