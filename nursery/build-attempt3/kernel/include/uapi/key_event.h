#ifndef KEY_EVENT_H
#define KEY_EVENT_H

// passed in in syscall
typedef struct key_event {
    uint16_t keycode;
    uint8_t ascii;
} key_event_t;



#endif