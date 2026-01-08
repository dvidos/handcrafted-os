#pragma once
#include "../concepts/keycodes.h"

void keyboard_driver_process();
keycode_t keyboard_keycode_from(int scancode, int is_e0);
char keyboard_ascii_from(keycode_t keycode, keymods_t modifiers);