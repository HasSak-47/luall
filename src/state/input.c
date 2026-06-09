#include <lauxlib.h>
#include <lua.h>

#include <input_key.h>
#include <state.h>

#include <string.h>
#include <unistd.h>

#define LUA_INPUT_KEY_MT "lyra.input_key"

static const char* input_key_kind_name(enum InputKeyKind kind) {
    switch (kind) {
    case INPUT_KEY_KIND_LETTER:
        return "letter";
    case INPUT_KEY_KIND_MODIFIER:
        return "modifier";
    case INPUT_KEY_KIND_SPECIAL:
        return "special";
    case INPUT_KEY_KIND_NONE:
    default:
        return "none";
    }
}

static const char* input_modifier_name(enum InputModifier modifier) {
    switch (modifier) {
    case INPUT_MODIFIER_SHIFT:
        return "shift";
    case INPUT_MODIFIER_ALT:
        return "alt";
    case INPUT_MODIFIER_CTRL:
        return "ctrl";
    case INPUT_MODIFIER_NONE:
    default:
        return NULL;
    }
}

static const char* input_special_name(enum InputSpecialKey special) {
    switch (special) {
    case INPUT_SPECIAL_UP:
        return "up";
    case INPUT_SPECIAL_DOWN:
        return "down";
    case INPUT_SPECIAL_RIGHT:
        return "right";
    case INPUT_SPECIAL_LEFT:
        return "left";
    case INPUT_SPECIAL_ENTER:
        return "enter";
    case INPUT_SPECIAL_TAB:
        return "tab";
    case INPUT_SPECIAL_BACKSPACE:
        return "backspace";
    case INPUT_SPECIAL_ESCAPE:
        return "escape";
    case INPUT_SPECIAL_DELETE:
        return "delete";
    case INPUT_SPECIAL_INSERT:
        return "insert";
    case INPUT_SPECIAL_HOME:
        return "home";
    case INPUT_SPECIAL_END:
        return "end";
    case INPUT_SPECIAL_PAGE_UP:
        return "page_up";
    case INPUT_SPECIAL_PAGE_DOWN:
        return "page_down";
    case INPUT_SPECIAL_F1:
        return "f1";
    case INPUT_SPECIAL_F2:
        return "f2";
    case INPUT_SPECIAL_F3:
        return "f3";
    case INPUT_SPECIAL_F4:
        return "f4";
    case INPUT_SPECIAL_F5:
        return "f5";
    case INPUT_SPECIAL_F6:
        return "f6";
    case INPUT_SPECIAL_F7:
        return "f7";
    case INPUT_SPECIAL_F8:
        return "f8";
    case INPUT_SPECIAL_F9:
        return "f9";
    case INPUT_SPECIAL_F10:
        return "f10";
    case INPUT_SPECIAL_F11:
        return "f11";
    case INPUT_SPECIAL_F12:
        return "f12";
    default:
        return NULL;
    }
}

static void push_nil_or_string(lua_State* L, const char* value) {
    if (value == NULL)
        lua_pushnil(L);
    else
        lua_pushstring(L, value);
}

static int index_input_key(lua_State* L) {
    struct InputKey* key = luaL_checkudata(L, 1, LUA_INPUT_KEY_MT);
    const char* name     = luaL_checkstring(L, 2);

    if (strcmp(name, "kind") == 0) {
        lua_pushstring(L, input_key_kind_name(key->kind));
    }
    else if (strcmp(name, "letter") == 0) {
        if (key->kind == INPUT_KEY_KIND_LETTER)
            lua_pushlstring(L, &key->value.letter, 1);
        else
            lua_pushnil(L);
    }
    else if (strcmp(name, "modifier") == 0) {
        if (key->kind == INPUT_KEY_KIND_MODIFIER)
            push_nil_or_string(L, input_modifier_name(key->value.modifier));
        else
            lua_pushnil(L);
    }
    else if (strcmp(name, "special") == 0) {
        if (key->kind == INPUT_KEY_KIND_SPECIAL)
            push_nil_or_string(L, input_special_name(key->value.special));
        else
            lua_pushnil(L);
    }
    else if (strcmp(name, "modifiers") == 0) {
        lua_pushinteger(L, key->modifiers);
    }
    else if (strcmp(name, "shift") == 0) {
        lua_pushboolean(L, input_key_has_modifier(*key, INPUT_MODIFIER_SHIFT));
    }
    else if (strcmp(name, "alt") == 0) {
        lua_pushboolean(L, input_key_has_modifier(*key, INPUT_MODIFIER_ALT));
    }
    else if (strcmp(name, "ctrl") == 0) {
        lua_pushboolean(L, input_key_has_modifier(*key, INPUT_MODIFIER_CTRL));
    }
    else {
        lua_pushnil(L);
    }

    return 1;
}

static int tostring_input_key(lua_State* L) {
    struct InputKey* key = luaL_checkudata(L, 1, LUA_INPUT_KEY_MT);

    switch (key->kind) {
    case INPUT_KEY_KIND_LETTER:
        lua_pushfstring(L, "InputKey{kind=%s, letter=%c, modifiers=%d}",
            input_key_kind_name(key->kind), key->value.letter, key->modifiers);
        break;
    case INPUT_KEY_KIND_MODIFIER:
        lua_pushfstring(L, "InputKey{kind=%s, modifier=%s}",
            input_key_kind_name(key->kind),
            input_modifier_name(key->value.modifier));
        break;
    case INPUT_KEY_KIND_SPECIAL:
        lua_pushfstring(L, "InputKey{kind=%s, special=%s, modifiers=%d}",
            input_key_kind_name(key->kind),
            input_special_name(key->value.special), key->modifiers);
        break;
    case INPUT_KEY_KIND_NONE:
    default:
        lua_pushstring(L, "InputKey{kind=none}");
        break;
    }

    return 1;
}

void create_input_key_metatable(lua_State* L) {
    if (luaL_newmetatable(L, LUA_INPUT_KEY_MT)) {
        lua_pushcfunction(L, index_input_key);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, tostring_input_key);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
}

int push_input_key(lua_State* L, struct InputKey* key) {
    struct InputKey* value = lua_newuserdata(L, sizeof(struct InputKey));
    *value                 = *key;
    luaL_getmetatable(L, LUA_INPUT_KEY_MT);
    lua_setmetatable(L, -2);

    return 1;
}

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

bool read_input_key(struct InputKey* key) {
    unsigned char byte = 0;
    if (read(STDIN_FILENO, &byte, 1) != 1)
        return false;

    if (byte == 0x1b)
        return read_escape_sequence(key);

    *key = decode_byte(byte);
    return key->kind != INPUT_KEY_KIND_NONE;
}
