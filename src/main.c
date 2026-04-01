#include <bindgen.h>
#include <debug.h>
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

    args = parse_args(argc, argv);

    if (is_debug(args)) {
        state.vars.debug = true;
        debug_printf("running in debug mode\n");
    }
    script = get_script(args);
    if (script != NULL) {
        debug_printf("running scripting %s\n", script);
    }

    init_shell_state();
    while (state.running) {
        run_hooks();
        if (state.reload) {
            end_shell_state();
            init_shell_state();
            state.reload = false;
        }
    }
    end_shell_state();
    return 0;
}
