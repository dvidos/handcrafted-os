#include "keycodes.h"
#include "../memory/string.h"

const char *keycode_string(keycode_t k) {
    switch (k) {
        case KEY_NONE: return "NONE";
        case KEY_A: return "A";
        case KEY_B: return "B";
        case KEY_C: return "C";
        case KEY_D: return "D";
        case KEY_E: return "E";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_I: return "I";
        case KEY_J: return "J";
        case KEY_K: return "K";
        case KEY_L: return "L";
        case KEY_M: return "M";
        case KEY_N: return "N";
        case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_Q: return "Q";
        case KEY_R: return "R";
        case KEY_S: return "S";
        case KEY_T: return "T";
        case KEY_U: return "U";
        case KEY_V: return "V";
        case KEY_W: return "W";
        case KEY_X: return "X";
        case KEY_Y: return "Y";
        case KEY_Z: return "Z";
        case KEY_0: return "0";
        case KEY_1: return "1";
        case KEY_2: return "2";
        case KEY_3: return "3";
        case KEY_4: return "4";
        case KEY_5: return "5";
        case KEY_6: return "6";
        case KEY_7: return "7";
        case KEY_8: return "8";
        case KEY_9: return "9";
        case KEY_BACKTICK: return "BACKTICK";
        case KEY_MINUS: return "MINUS";
        case KEY_EQUAL: return "EQUAL";
        case KEY_LBRACKET: return "LBRACKET";
        case KEY_RBRACKET: return "RBRACKET";
        case KEY_BACKSLASH: return "BACKSLASH";
        case KEY_SEMICOLON: return "SEMICOLON";
        case KEY_APOSTROPHE: return "APOSTROPHE";
        case KEY_COMMA: return "COMMA";
        case KEY_DOT: return "DOT";
        case KEY_SLASH: return "SLASH";
        case KEY_SPACE: return "SPACE";
        case KEY_ENTER: return "ENTER";
        case KEY_ESCAPE: return "ESCAPE";
        case KEY_BACKSPACE: return "BACKSPACE";
        case KEY_TAB: return "TAB";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_F11: return "F11";
        case KEY_F12: return "F12";
        case KEY_SHIFT: return "SHIFT";
        case KEY_CTRL: return "CTRL";
        case KEY_ALT: return "ALT";
        case KEY_SUPER: return "SUPER";
        case KEY_CAPSLOCK: return "CAPSLOCK";
        case KEY_NUMLOCK: return "NUMLOCK";
        case KEY_SCROLLLOCK: return "SCROLLLOCK";
        case KEY_INSERT: return "INSERT";
        case KEY_DELETE: return "DELETE";
        case KEY_HOME: return "HOME";
        case KEY_END: return "END";
        case KEY_PAGEUP: return "PAGEUP";
        case KEY_PAGEDOWN: return "PAGEDOWN";
        case KEY_UP: return "UP";
        case KEY_DOWN: return "DOWN";
        case KEY_LEFT: return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        case KEY_KP_DIV: return "KP_DIV";
        case KEY_KP_MUL: return "KP_MUL";
        case KEY_KP_MINUS: return "KP_MINUS";
        case KEY_KP_PLUS: return "KP_PLUS";
        case KEY_KP_ENTER: return "KP_ENTER";
        case KEY_KP_DOT: return "KP_DOT";
        case KEY_KP_0: return "KP_0";
        case KEY_KP_1: return "KP_1";
        case KEY_KP_2: return "KP_2";
        case KEY_KP_3: return "KP_3";
        case KEY_KP_4: return "KP_4";
        case KEY_KP_5: return "KP_5";
        case KEY_KP_6: return "KP_6";
        case KEY_KP_7: return "KP_7";
        case KEY_KP_8: return "KP_8";
        case KEY_KP_9: return "KP_9";
        default: return "(unknown)";
    }
}

const char *keymods_string(keymods_t mods) {
    static char buffer[32];

    buffer[0] = 0;
    if (mods & MOD_CTRL) {
        if (buffer[0]) strcat(buffer, "+");
        strcat(buffer, "CTRL");
    }
    if (mods & MOD_ALT) {
        if (buffer[0]) strcat(buffer, "+");
        strcat(buffer, "ALT");
    }
    if (mods & MOD_SHIFT) {
        if (buffer[0]) strcat(buffer, "+");
        strcat(buffer, "SHIFT");
    }
    if (mods & MOD_SUPER) {
        if (buffer[0]) strcat(buffer, "+");
        strcat(buffer, "SUPER");
    }
    if (buffer[0]) strcat(buffer, "+");

    return buffer;
}