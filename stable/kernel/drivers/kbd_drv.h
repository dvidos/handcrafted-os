#pragma once


#include "../include/ctypes.h"
#include "../arch/stack_frames.h"
#include "../include/uapi/key_event.h"


typedef void (key_event_hook_t)(key_event_t *event, bool *handled);
bool keyboard_register_hook(key_event_hook_t hook);
void keyboard_unregister_hook(key_event_hook_t hook);

void keyboard_handler(interrupt_frame_t* regs);
void reboot();

bool kbd_get_event_non_blocking(key_event_t *event);



