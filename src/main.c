#include <bindgen.h>
#include <debug.h>
#include <input_key.h>
#include <path.h>
#include <state.h>
#include <stdio.h>
#include <utils.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <dlfcn.h>
#include <termios.h>
#include <unistd.h>

static uint8_t parse_modifier_code(int code) {
    switch (code) {
    case 2:
        return INPUT_MODIFIER_SHIFT;
    case 3:
        return INPUT_MODIFIER_ALT;
    case 4:
        return INPUT_MODIFIER_SHIFT | INPUT_MODIFIER_ALT;
    case 5:
        return INPUT_MODIFIER_CTRL;
    case 6:
        return INPUT_MODIFIER_SHIFT | INPUT_MODIFIER_CTRL;
    case 7:
        return INPUT_MODIFIER_ALT | INPUT_MODIFIER_CTRL;
    case 8:
        return INPUT_MODIFIER_SHIFT | INPUT_MODIFIER_ALT | INPUT_MODIFIER_CTRL;
    default:
        return INPUT_MODIFIER_NONE;
    }
}

static enum InputSpecialKey parse_csi_tilde_key(int code) {
    switch (code) {
    case 1:
    case 7:
        return INPUT_SPECIAL_HOME;
    case 2:
        return INPUT_SPECIAL_INSERT;
    case 3:
        return INPUT_SPECIAL_DELETE;
    case 4:
    case 8:
        return INPUT_SPECIAL_END;
    case 5:
        return INPUT_SPECIAL_PAGE_UP;
    case 6:
        return INPUT_SPECIAL_PAGE_DOWN;
    case 15:
        return INPUT_SPECIAL_F5;
    case 17:
        return INPUT_SPECIAL_F6;
    case 18:
        return INPUT_SPECIAL_F7;
    case 19:
        return INPUT_SPECIAL_F8;
    case 20:
        return INPUT_SPECIAL_F9;
    case 21:
        return INPUT_SPECIAL_F10;
    case 23:
        return INPUT_SPECIAL_F11;
    case 24:
        return INPUT_SPECIAL_F12;
    default:
        return -1;
    }
}

static struct InputKey decode_byte(unsigned char byte) {
    if (byte == '\r' || byte == '\n')
        return input_key_special(INPUT_SPECIAL_ENTER);
    if (byte == '\t')
        return input_key_special(INPUT_SPECIAL_TAB);
    if (byte == 0x08 || byte == 0x7f)
        return input_key_special(INPUT_SPECIAL_BACKSPACE);
    if (byte >= 1 && byte <= 26)
        return input_key_modified_letter('a' + (byte - 1), INPUT_MODIFIER_CTRL);
    if (byte >= 32 && byte <= 126)
        return input_key_letter((char)byte);

    return input_key_none();
}

static bool read_escape_sequence(struct InputKey* key) {
    unsigned char first = 0;
    if (read(STDIN_FILENO, &first, 1) != 1) {
        *key = input_key_special(INPUT_SPECIAL_ESCAPE);
        return true;
    }

    if (first == '[' || first == 'O') {
        char sequence[16] = {0};
        size_t len        = 0;

        while (len < sizeof(sequence) - 1) {
            unsigned char next = 0;
            if (read(STDIN_FILENO, &next, 1) != 1)
                break;

            sequence[len++] = (char)next;
            if (next >= 0x40 && next <= 0x7e)
                break;
        }

        if (len == 0) {
            *key = input_key_special(INPUT_SPECIAL_ESCAPE);
            return true;
        }

        char final          = sequence[len - 1];
        int params[3]       = {0};
        size_t param_count  = 0;
        int current         = 0;
        bool reading_number = false;

        for (size_t i = 0; i + 1 < len && param_count < 3; ++i) {
            char ch = sequence[i];
            if (ch >= '0' && ch <= '9') {
                current        = current * 10 + (ch - '0');
                reading_number = true;
                continue;
            }
            if (ch == ';') {
                if (reading_number)
                    params[param_count++] = current;
                current        = 0;
                reading_number = false;
            }
        }
        if (reading_number && param_count < 3)
            params[param_count++] = current;

        uint8_t modifiers = INPUT_MODIFIER_NONE;
        if (param_count >= 2)
            modifiers = parse_modifier_code(params[param_count - 1]);

        switch (final) {
        case 'A':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_UP), modifiers);
            return true;
        case 'B':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_DOWN), modifiers);
            return true;
        case 'C':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_RIGHT), modifiers);
            return true;
        case 'D':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_LEFT), modifiers);
            return true;
        case 'H':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_HOME), modifiers);
            return true;
        case 'F':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_END), modifiers);
            return true;
        case 'P':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_F1), modifiers);
            return true;
        case 'Q':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_F2), modifiers);
            return true;
        case 'R':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_F3), modifiers);
            return true;
        case 'S':
            *key = input_key_with_modifiers(
                input_key_special(INPUT_SPECIAL_F4), modifiers);
            return true;
        case '~':
            if (param_count >= 1) {
                enum InputSpecialKey special = parse_csi_tilde_key(params[0]);
                if ((int)special != -1) {
                    *key = input_key_with_modifiers(
                        input_key_special(special), modifiers);
                    return true;
                }
            }
            break;
        }

        *key = input_key_special(INPUT_SPECIAL_ESCAPE);
        return true;
    }

    *key = decode_byte(first);
    if (key->kind == INPUT_KEY_KIND_NONE)
        *key = input_key_special(INPUT_SPECIAL_ESCAPE);
    else
        key->modifiers |= INPUT_MODIFIER_ALT;

    return true;
}

static bool read_input_key(struct InputKey* key) {
    unsigned char byte = 0;
    if (read(STDIN_FILENO, &byte, 1) != 1)
        return false;

    if (byte == 0x1b)
        return read_escape_sequence(key);

    *key = decode_byte(byte);
    return key->kind != INPUT_KEY_KIND_NONE;
}

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
