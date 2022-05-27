#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "../idt.h"


#define KEY_ESCAPE             1
#define KEY_ENTER              2
#define KEY_BACKSPACE          3
#define KEY_TAB                4
#define KEY_SPACE              5
#define KEY_F1                 6
#define KEY_F2                 7
#define KEY_F3                 8
#define KEY_F4                 9
#define KEY_F5                10
#define KEY_F6                11
#define KEY_F7                12
#define KEY_F8                13
#define KEY_F9                14
#define KEY_F10               15
#define KEY_F11               16
#define KEY_F12               17
#define KEY_UP                18
#define KEY_DOWN              19
#define KEY_LEFT              20
#define KEY_RIGHT             21
#define KEY_HOME              22
#define KEY_END               23
#define KEY_PAGE_UP           24
#define KEY_PAGE_DOWN         25
#define KEY_INSERT            26
#define KEY_DELETE            27
#define KEY_KP_STAR           28
#define KEY_KP_PLUS           29
#define KEY_KP_MINUS          30
#define KEY_KP_SLASH          31
#define KEY_KP_1              32
#define KEY_KP_2              33
#define KEY_KP_3              34
#define KEY_KP_4              35
#define KEY_KP_5              36
#define KEY_KP_6              37
#define KEY_KP_7              38
#define KEY_KP_8              39
#define KEY_KP_9              30
#define KEY_KP_0              41
#define KEY_KP_DECIMAL        42
#define KEY_KP_ENTER          43
#define KEY_MEDIA_PREV        44
#define KEY_MEDIA_NEXT        45
#define KEY_MEDIA_MUTE        46
#define KEY_MEDIA_CALC        47
#define KEY_MEDIA_PLAY        48
#define KEY_MEDIA_STOP        49
#define KEY_MEDIA_VOL_DOWN    50
#define KEY_MEDIA_VOL_UP      51
#define KEY_MEDIA_WWW         52
#define KEY_MEDIA_GUI_LEFT    53 // window logo
#define KEY_MEDIA_GUI_RIGHT   54
#define KEY_MEDIA_APPS        55 // context menu

#include <stdint.h>

struct key_event {
    uint8_t printable; // e.g. 'A' (in the future, support for utf8) - zero if no printable for this keystroke
    uint8_t special_key; // e.g. KEY_PAGE_DOWN or multimedia
    uint8_t ctrl_down: 1;
    uint8_t alt_down: 1;
    uint8_t shift_down: 1;
} __attribute__((packed));

typedef struct key_event key_event_t;

void keyboard_handler(registers_t* regs);
void wait_keyboard_event(key_event_t *event);

#endif
