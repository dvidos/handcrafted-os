#pragma once

typedef enum keycode {
    KEY_NONE = 0,

    // Letters
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
    KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,
    KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X,
    KEY_Y, KEY_Z,

    // Numbers
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7,
    KEY_8, KEY_9,

    // Symbols
    KEY_MINUS, KEY_EQUAL, KEY_LBRACKET, KEY_RBRACKET,
    KEY_BACKSLASH, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE,
    KEY_COMMA, KEY_DOT, KEY_SLASH,

    // Whitespace / control
    KEY_SPACE, KEY_ENTER, KEY_ESCAPE, KEY_BACKSPACE, KEY_TAB,

    // Function keys
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

    // Modifier keys
    KEY_SHIFT, KEY_CTRL, KEY_ALT, KEY_SUPER,
    KEY_CAPSLOCK, KEY_NUMLOCK, KEY_SCROLLLOCK,

    // Navigation / keypad (E0)
    KEY_INSERT, KEY_DELETE, KEY_HOME, KEY_END,
    KEY_PAGEUP, KEY_PAGEDOWN,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,

    KEY_KP_DIV, KEY_KP_MUL, KEY_KP_MINUS, KEY_KP_PLUS,
    KEY_KP_ENTER, KEY_KP_DOT,
    KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,

} keycode_t;

typedef enum {
    MOD_NONE  = 0,
    MOD_SHIFT = 1 << 0,
    MOD_CTRL  = 1 << 1,
    MOD_ALT   = 1 << 2,
    MOD_SUPER = 1 << 3,
} keymods_t;
