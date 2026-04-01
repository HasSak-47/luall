#include <bindgen.h>
#include <debug.h>
#include <path.h>
#include <state.h>
#include <utils.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <dlfcn.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;
bool got_original = false;

void unset_raw_mode() {
    if (got_original)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    else
        debug_printf("there is no original termios?\n");
}

void set_raw_mode() {
    if (!got_original) {
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(unset_raw_mode);
        got_original = true;
    }

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

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
    return 0;
}
