#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include "../include/ctypes.h"
#include "../arch/trap_frame.h"
#include "../include/uapi/key_event.h"


typedef void (*key_event_hook_t)(key_event_t *event, bool *handled);
void keyboard_register_hook(key_event_hook_t hook);
void keyboard_unregister_hook(key_event_hook_t hook);

void keyboard_handler(trap_frame_t* regs);
void reboot();



#endif
