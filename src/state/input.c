#include <lauxlib.h>
#include <lua.h>

#include <input_key.h>
#include <state.h>

#include <string.h>

#define LUA_INPUT_KEY_MT "rewsh.input_key"

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

void push_input_key(lua_State* L, struct InputKey key) {
    struct InputKey* value = lua_newuserdata(L, sizeof(struct InputKey));
    *value                 = key;
    luaL_getmetatable(L, LUA_INPUT_KEY_MT);
    lua_setmetatable(L, -2);
}
