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
#include <termios.h>
#include <unistd.h>

int main(const int argc, const char* argv[]) {
    init_logger();

#ifdef LY_TEST
    test_handler(argc, argv);
    return 0;
#else

    struct Args* args  = NULL;
    const char* script = NULL;

    args = args_parse(argc, argv);

    state.vars.log_level = args_get_level(args);
    set_log_level(state.vars.log_level);

    if (state.vars.log_level >= LEVEL_DEBUG) {
        debug_printf("running in debug mode");
    }

    script = args_get_script(args);
    if (script != NULL) {
        debug_printf("running scripting %s", script);
    }

    args_delete(args);

    debug_printf("starting shell state");
    init_shell_state();

    debug_printf("started event loop");
    trigger_enter_hook();
    struct InputKey key = input_key_none();

    while (state.is_running) {
        if (state.reload) {
            debug_printf("reloading...");
            debug_printf("running exit hooks before reload...");
            trigger_exit_hook();
            debug_printf("ending current state...");
            end_shell_state();
            debug_printf("initing current state...");
            init_shell_state();
            debug_printf("running enter hooks after reload...");
            trigger_enter_hook();
            state.reload = false;
        }

        if (read_input_key(&key)) {
            trigger_input_hook(key);
        }
    }
    trigger_exit_hook();
    end_shell_state();
    return 0;
#endif
}
