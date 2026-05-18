#include <logs.h>
#include <term.h>

#include <termios.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdlib.h>

static struct termios orig_termios;
static bool got_original = false;

void unset_raw_mode() {
    if (got_original)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    else
        log_warn("there is no original termios?");
}

void set_raw_mode() {
    log_debug("entering raw mode...");
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
    log_debug("entered raw mode");
}

void enter_alternate_screen() {
    write(STDOUT_FILENO, "\x1b[?1049h", 8);
    log_debug("entered alternate screen");
}

void leave_alternate_screen() {
    write(STDOUT_FILENO, "\x1b[?1049l", 8);
    log_debug("left alternate screen");
}
