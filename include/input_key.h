#ifndef __INPUT_KEY_H__
#define __INPUT_KEY_H__

#include <stdbool.h>
#include <stdint.h>

enum InputKeyKind {
    INPUT_KEY_KIND_NONE = 0,
    INPUT_KEY_KIND_LETTER,
    INPUT_KEY_KIND_MODIFIER,
    INPUT_KEY_KIND_SPECIAL,
};

enum InputModifier {
    INPUT_MODIFIER_NONE  = 0,
    INPUT_MODIFIER_SHIFT = 1 << 0,
    INPUT_MODIFIER_ALT   = 1 << 1,
    INPUT_MODIFIER_CTRL  = 1 << 2,
};

enum InputSpecialKey {
    INPUT_SPECIAL_UP = 0,
    INPUT_SPECIAL_DOWN,
    INPUT_SPECIAL_RIGHT,
    INPUT_SPECIAL_LEFT,
    INPUT_SPECIAL_ENTER,
    INPUT_SPECIAL_TAB,
    INPUT_SPECIAL_BACKSPACE,
    INPUT_SPECIAL_ESCAPE,
    INPUT_SPECIAL_DELETE,
    INPUT_SPECIAL_INSERT,
    INPUT_SPECIAL_HOME,
    INPUT_SPECIAL_END,
    INPUT_SPECIAL_PAGE_UP,
    INPUT_SPECIAL_PAGE_DOWN,
    INPUT_SPECIAL_F1,
    INPUT_SPECIAL_F2,
    INPUT_SPECIAL_F3,
    INPUT_SPECIAL_F4,
    INPUT_SPECIAL_F5,
    INPUT_SPECIAL_F6,
    INPUT_SPECIAL_F7,
    INPUT_SPECIAL_F8,
    INPUT_SPECIAL_F9,
    INPUT_SPECIAL_F10,
    INPUT_SPECIAL_F11,
    INPUT_SPECIAL_F12,
};

struct InputKey {
    enum InputKeyKind kind;
    uint8_t modifiers;

    union {
        char letter;
        enum InputModifier modifier;
        enum InputSpecialKey special;
    } value;
};

static inline struct InputKey input_key_none() {
    struct InputKey key = {
        .kind      = INPUT_KEY_KIND_NONE,
        .modifiers = INPUT_MODIFIER_NONE,
        .value     = {.letter = 0},
    };
    return key;
}

static inline struct InputKey input_key_letter(char letter) {
    struct InputKey key = {
        .kind      = INPUT_KEY_KIND_LETTER,
        .modifiers = INPUT_MODIFIER_NONE,
        .value     = {.letter = letter},
    };
    return key;
}

static inline struct InputKey input_key_modified_letter(
    char letter, uint8_t modifiers) {
    struct InputKey key = {
        .kind      = INPUT_KEY_KIND_LETTER,
        .modifiers = modifiers,
        .value     = {.letter = letter},
    };
    return key;
}

static inline struct InputKey input_key_modifier(enum InputModifier modifier) {
    struct InputKey key = {
        .kind      = INPUT_KEY_KIND_MODIFIER,
        .modifiers = INPUT_MODIFIER_NONE,
        .value     = {.modifier = modifier},
    };
    return key;
}

static inline struct InputKey input_key_special(enum InputSpecialKey special) {
    struct InputKey key = {
        .kind      = INPUT_KEY_KIND_SPECIAL,
        .modifiers = INPUT_MODIFIER_NONE,
        .value     = {.special = special},
    };
    return key;
}

static inline struct InputKey input_key_with_modifiers(
    struct InputKey key, uint8_t modifiers) {
    key.modifiers = modifiers;
    return key;
}

static inline bool input_key_has_modifier(
    struct InputKey key, enum InputModifier modifier) {
    return (key.modifiers & modifier) != 0;
}

#endif
