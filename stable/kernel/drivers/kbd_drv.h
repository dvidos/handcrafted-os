#pragma once


#include "../include/ctypes.h"
#include "../arch/stack_frames.h"
#include "../include/uapi/key_event.h"


typedef void (key_event_hook_t)(key_event_t *event, bool *handled);
bool keyboard_register_hook(key_event_hook_t hook);
void keyboard_unregister_hook(key_event_hook_t hook);

void keyboard_interrupt_handler(interrupt_frame_t* regs);
void reboot();

void kbd_wait_get_event(key_event_t *event);



