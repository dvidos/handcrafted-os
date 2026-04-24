#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include <ctypes.h>
#include <idt.h>
#include <keyboard.h>  // <-- this is keyboard.h from libc, to get keys designation and structure


typedef void (*key_event_hook_t)(key_event_t *event, bool *handled);
void keyboard_register_hook(key_event_hook_t hook);
void keyboard_unregister_hook(key_event_hook_t hook);

void keyboard_handler(registers_t* regs);
void reboot();



#endif
