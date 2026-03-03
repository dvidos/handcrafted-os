#include <keyboard.h>
#include <syscall.h>
// key codes and key_event_t defined in keyboard.h



void getkey(key_event_t *event) {
    syscall(SYS_GET_KEY_EVENT, (int)event, 0, 0, 0, 0);
}


