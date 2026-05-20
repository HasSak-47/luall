#include <bindgen_cli.h>
#include <debug.h>
#include <input_key.h>
#include <logs.h>
#include <path.h>
#include <state.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int main(const int argc, const char* argv[]) {
    init_logger(STDOUT_FILENO);

#ifdef LY_TEST
    test_handler(argc, argv);
    return 0;
#else

    struct Args* args       = NULL;
    const char* script      = NULL;
    const char* config_path = NULL;
    const char* cache_path  = NULL;
    char* config_path_copy  = NULL;
    char* cache_path_copy   = NULL;

    args = args_parse(argc, argv);

    state.vars.log_level = args_get_level(args);
    set_log_level(state.vars.log_level);

    if (state.vars.log_level >= LEVEL_DEBUG) {
        log_debug("running in debug mode");
    }

    script = args_get_script(args);
    if (script != NULL) {
        log_debug("running scripting %s", script);
    }

    config_path = args_get_config_path(args);
    cache_path  = args_get_cache_path(args);
    if (config_path != NULL) {
        config_path_copy = strdup(config_path);
    }
    if (cache_path != NULL) {
        cache_path_copy = strdup(cache_path);
    }

    args_delete(args);

    log_debug("starting shell state");
    init_shell_state(config_path_copy, cache_path_copy);

    trigger_enter_hook();
    log_debug("started first input event");
    struct InputKey key = input_key_none();

    log_debug("running...");
    while (state.is_running) {
        if (state.reload) {
            log_debug("reloading...");
            log_debug("running exit hooks before reload...");
            trigger_exit_hook();
            log_debug("ending current state...");
            end_shell_state();
            log_debug("initing current state...");
            init_shell_state(config_path_copy, cache_path_copy);
            log_debug("running enter hooks after reload...");
            trigger_enter_hook();
            state.reload = false;
        }

        if (read_input_key(&key)) {
            trigger_input_hook(key);
        }
    }
    log_debug("running exit hooks");
    trigger_exit_hook();
    end_shell_state();
    free(config_path_copy);
    free(cache_path_copy);
    return 0;
#endif
}
