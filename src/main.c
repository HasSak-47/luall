#include <bindgen.h>
#include <debug.h>
#include <input_key.h>
#include <path.h>
#include <state.h>
#include <utils.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <dlfcn.h>
#include <termios.h>
#include <unistd.h>

int main(const int argc, const char* argv[]) {

#ifdef LY_TEST
    test_handler(argc, argv);
    return 0;
#endif

    struct Args* args  = NULL;
    const char* script = NULL;

    debug_printf("parsing args...\n");
    args = parse_args(argc, argv);

    if (is_debug(args)) {
        state.vars.debug = true;
        debug_printf("running in debug mode\n");
    }
    script = get_script(args);
    if (script != NULL) {
        debug_printf("running scripting %s\n", script);
    }

    debug_printf("starting shell state\n");
    init_shell_state();

    debug_printf("started event loop\n");
    trigger_enter_hook();
    struct InputKey key = input_key_none();

    while (state.running) {
        if (read_input_key(&key)) {
            debug_printf("running key event");
            trigger_input_hook(key);
        }
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
    }
    trigger_exit_hook();
    end_shell_state();
    return 0;
}
