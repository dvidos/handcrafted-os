#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include <ctypes.h>


#define KEY_ESCAPE          0x01
#define KEY_ENTER           0x02
#define KEY_BACKSPACE       0x03
#define KEY_TAB             0x04
#define KEY_SPACE           0x05
#define KEY_F1              0x06
#define KEY_F2              0x07
#define KEY_F3              0x08
#define KEY_F4              0x09
#define KEY_F5              0x0a
#define KEY_F6              0x0b
#define KEY_F7              0x0c
#define KEY_F8              0x0d
#define KEY_F9              0x0e
#define KEY_F10             0x0f
#define KEY_F11             0x10
#define KEY_F12             0x11
#define KEY_UP              0x12
#define KEY_DOWN            0x13
#define KEY_LEFT            0x14
#define KEY_RIGHT           0x15
#define KEY_HOME            0x16
#define KEY_END             0x17
#define KEY_PAGE_UP         0x18
#define KEY_PAGE_DOWN       0x19
#define KEY_INSERT          0x1a
#define KEY_DELETE          0x1b
#define KEY_KP_STAR         0x1c
#define KEY_KP_PLUS         0x1d
#define KEY_KP_MINUS        0x1e
#define KEY_KP_SLASH        0x1f
#define KEY_KP_1            0x20
#define KEY_KP_2            0x21
#define KEY_KP_3            0x22
#define KEY_KP_4            0x23
#define KEY_KP_5            0x24
#define KEY_KP_6            0x25
#define KEY_KP_7            0x26
#define KEY_KP_8            0x27
#define KEY_KP_9            0x28
#define KEY_KP_0            0x29
#define KEY_KP_DECIMAL      0x2a
#define KEY_KP_ENTER        0x2b
#define KEY_MEDIA_PREV      0x2c
#define KEY_MEDIA_NEXT      0x2d
#define KEY_MEDIA_MUTE      0x2e
#define KEY_MEDIA_CALC      0x2f
#define KEY_MEDIA_PLAY      0x30
#define KEY_MEDIA_STOP      0x31
#define KEY_MEDIA_VOL_DOWN  0x32
#define KEY_MEDIA_VOL_UP    0x33
#define KEY_MEDIA_WWW       0x34
#define KEY_MEDIA_GUI_LEFT  0x35 // window logo
#define KEY_MEDIA_GUI_RIGHT 0x36
#define KEY_MEDIA_APPS      0x37 // context menu
#define KEY_SUPER           0x38
#define KEY_A               0x39
#define KEY_B               0x3a
#define KEY_C               0x3b
#define KEY_D               0x3c
#define KEY_E               0x3d
#define KEY_F               0x3e
#define KEY_G               0x3f
#define KEY_H               0x40
#define KEY_I               0x41
#define KEY_J               0x42
#define KEY_K               0x43
#define KEY_L               0x44
#define KEY_M               0x45
#define KEY_N               0x46
#define KEY_O               0x47
#define KEY_P               0x48
#define KEY_Q               0x49
#define KEY_R               0x4a
#define KEY_S               0x4b
#define KEY_T               0x4c
#define KEY_U               0x4d
#define KEY_V               0x4e
#define KEY_W               0x4f
#define KEY_X               0x50
#define KEY_Y               0x51
#define KEY_Z               0x52
#define KEY_BACKTICK        0x53
#define KEY_1               0x54
#define KEY_2               0x55
#define KEY_3               0x56
#define KEY_4               0x57
#define KEY_5               0x58
#define KEY_6               0x59
#define KEY_7               0x5a
#define KEY_8               0x5b
#define KEY_9               0x5c
#define KEY_0               0x5d
#define KEY_MINUS           0x5e
#define KEY_EQUAL           0x5f
#define KEY_LEFT_SQ_BRKT    0x60
#define KEY_RIGHT_SQ_BRKT   0x61
#define KEY_BACKSLASH       0x62
#define KEY_SEMICOLON       0x63
#define KEY_QUOTE           0x64
#define KEY_PERIOD          0x65
#define KEY_COMMA           0x66
#define KEY_SLASH           0x67
#define KEY_CAPS_LOCK       0x68
#define KEY_SHIFT           0x69
#define KEY_CONTROL         0x6a
#define KEY_ALT             0x6b

// modifiers and keys will be packed in a 16 bit word, to allow for specific values:
#define MOD_SHIFT           0x0100
#define MOD_CTRL            0x0200
#define MOD_ALT             0x0400
#define MOD_SUPER           0x0800

#define KEY_CTRL_A                (MOD_CTRL | KEY_A)
#define KEY_CTRL_B                (MOD_CTRL | KEY_B)
#define KEY_CTRL_C                (MOD_CTRL | KEY_C)
#define KEY_CTRL_D                (MOD_CTRL | KEY_D)
#define KEY_CTRL_E                (MOD_CTRL | KEY_E)
#define KEY_CTRL_F                (MOD_CTRL | KEY_F)
#define KEY_CTRL_G                (MOD_CTRL | KEY_G)
#define KEY_CTRL_H                (MOD_CTRL | KEY_H)
#define KEY_CTRL_I                (MOD_CTRL | KEY_I)
#define KEY_CTRL_J                (MOD_CTRL | KEY_J)
#define KEY_CTRL_K                (MOD_CTRL | KEY_K)
#define KEY_CTRL_L                (MOD_CTRL | KEY_L)
#define KEY_CTRL_M                (MOD_CTRL | KEY_M)
#define KEY_CTRL_N                (MOD_CTRL | KEY_N)
#define KEY_CTRL_O                (MOD_CTRL | KEY_O)
#define KEY_CTRL_P                (MOD_CTRL | KEY_P)
#define KEY_CTRL_Q                (MOD_CTRL | KEY_Q)
#define KEY_CTRL_R                (MOD_CTRL | KEY_R)
#define KEY_CTRL_S                (MOD_CTRL | KEY_S)
#define KEY_CTRL_T                (MOD_CTRL | KEY_T)
#define KEY_CTRL_U                (MOD_CTRL | KEY_U)
#define KEY_CTRL_V                (MOD_CTRL | KEY_V)
#define KEY_CTRL_W                (MOD_CTRL | KEY_W)
#define KEY_CTRL_X                (MOD_CTRL | KEY_X)
#define KEY_CTRL_Y                (MOD_CTRL | KEY_Y)
#define KEY_CTRL_Z                (MOD_CTRL | KEY_Z)

#define KEY_CTRL_1                (MOD_CTRL | KEY_1)
#define KEY_CTRL_2                (MOD_CTRL | KEY_2)
#define KEY_CTRL_3                (MOD_CTRL | KEY_3)
#define KEY_CTRL_4                (MOD_CTRL | KEY_4)
#define KEY_CTRL_5                (MOD_CTRL | KEY_5)
#define KEY_CTRL_6                (MOD_CTRL | KEY_6)
#define KEY_CTRL_7                (MOD_CTRL | KEY_7)
#define KEY_CTRL_8                (MOD_CTRL | KEY_8)
#define KEY_CTRL_9                (MOD_CTRL | KEY_9)
#define KEY_CTRL_0                (MOD_CTRL | KEY_0)

#define KEY_CTRL_SHIFT_A          (MOD_CTRL | MOD_SHIFT | KEY_A)
#define KEY_CTRL_SHIFT_B          (MOD_CTRL | MOD_SHIFT | KEY_B)
#define KEY_CTRL_SHIFT_C          (MOD_CTRL | MOD_SHIFT | KEY_C)
#define KEY_CTRL_SHIFT_D          (MOD_CTRL | MOD_SHIFT | KEY_D)
#define KEY_CTRL_SHIFT_E          (MOD_CTRL | MOD_SHIFT | KEY_E)
#define KEY_CTRL_SHIFT_F          (MOD_CTRL | MOD_SHIFT | KEY_F)
#define KEY_CTRL_SHIFT_G          (MOD_CTRL | MOD_SHIFT | KEY_G)
#define KEY_CTRL_SHIFT_H          (MOD_CTRL | MOD_SHIFT | KEY_H)
#define KEY_CTRL_SHIFT_I          (MOD_CTRL | MOD_SHIFT | KEY_I)
#define KEY_CTRL_SHIFT_J          (MOD_CTRL | MOD_SHIFT | KEY_J)
#define KEY_CTRL_SHIFT_K          (MOD_CTRL | MOD_SHIFT | KEY_K)
#define KEY_CTRL_SHIFT_L          (MOD_CTRL | MOD_SHIFT | KEY_L)
#define KEY_CTRL_SHIFT_M          (MOD_CTRL | MOD_SHIFT | KEY_M)
#define KEY_CTRL_SHIFT_N          (MOD_CTRL | MOD_SHIFT | KEY_N)
#define KEY_CTRL_SHIFT_O          (MOD_CTRL | MOD_SHIFT | KEY_O)
#define KEY_CTRL_SHIFT_P          (MOD_CTRL | MOD_SHIFT | KEY_P)
#define KEY_CTRL_SHIFT_Q          (MOD_CTRL | MOD_SHIFT | KEY_Q)
#define KEY_CTRL_SHIFT_R          (MOD_CTRL | MOD_SHIFT | KEY_R)
#define KEY_CTRL_SHIFT_S          (MOD_CTRL | MOD_SHIFT | KEY_S)
#define KEY_CTRL_SHIFT_T          (MOD_CTRL | MOD_SHIFT | KEY_T)
#define KEY_CTRL_SHIFT_U          (MOD_CTRL | MOD_SHIFT | KEY_U)
#define KEY_CTRL_SHIFT_V          (MOD_CTRL | MOD_SHIFT | KEY_V)
#define KEY_CTRL_SHIFT_W          (MOD_CTRL | MOD_SHIFT | KEY_W)
#define KEY_CTRL_SHIFT_X          (MOD_CTRL | MOD_SHIFT | KEY_X)
#define KEY_CTRL_SHIFT_Y          (MOD_CTRL | MOD_SHIFT | KEY_Y)
#define KEY_CTRL_SHIFT_Z          (MOD_CTRL | MOD_SHIFT | KEY_Z)

#define KEY_ALT_A                 (MOD_ALT | KEY_A)
#define KEY_ALT_B                 (MOD_ALT | KEY_B)
#define KEY_ALT_C                 (MOD_ALT | KEY_C)
#define KEY_ALT_D                 (MOD_ALT | KEY_D)
#define KEY_ALT_E                 (MOD_ALT | KEY_E)
#define KEY_ALT_F                 (MOD_ALT | KEY_F)
#define KEY_ALT_G                 (MOD_ALT | KEY_G)
#define KEY_ALT_H                 (MOD_ALT | KEY_H)
#define KEY_ALT_I                 (MOD_ALT | KEY_I)
#define KEY_ALT_J                 (MOD_ALT | KEY_J)
#define KEY_ALT_K                 (MOD_ALT | KEY_K)
#define KEY_ALT_L                 (MOD_ALT | KEY_L)
#define KEY_ALT_M                 (MOD_ALT | KEY_M)
#define KEY_ALT_N                 (MOD_ALT | KEY_N)
#define KEY_ALT_O                 (MOD_ALT | KEY_O)
#define KEY_ALT_P                 (MOD_ALT | KEY_P)
#define KEY_ALT_Q                 (MOD_ALT | KEY_Q)
#define KEY_ALT_R                 (MOD_ALT | KEY_R)
#define KEY_ALT_S                 (MOD_ALT | KEY_S)
#define KEY_ALT_T                 (MOD_ALT | KEY_T)
#define KEY_ALT_U                 (MOD_ALT | KEY_U)
#define KEY_ALT_V                 (MOD_ALT | KEY_V)
#define KEY_ALT_W                 (MOD_ALT | KEY_W)
#define KEY_ALT_X                 (MOD_ALT | KEY_X)
#define KEY_ALT_Y                 (MOD_ALT | KEY_Y)
#define KEY_ALT_Z                 (MOD_ALT | KEY_Z)

#define KEY_ALT_1                 (MOD_ALT | KEY_1)
#define KEY_ALT_2                 (MOD_ALT | KEY_2)
#define KEY_ALT_3                 (MOD_ALT | KEY_3)
#define KEY_ALT_4                 (MOD_ALT | KEY_4)
#define KEY_ALT_5                 (MOD_ALT | KEY_5)
#define KEY_ALT_6                 (MOD_ALT | KEY_6)
#define KEY_ALT_7                 (MOD_ALT | KEY_7)
#define KEY_ALT_8                 (MOD_ALT | KEY_8)
#define KEY_ALT_9                 (MOD_ALT | KEY_9)
#define KEY_ALT_0                 (MOD_ALT | KEY_0)

#define KEY_SHIFT_SLASH           (MOD_SHIFT | KEY_SLASH)
#define KEY_SHIFT_TAB             (MOD_SHIFT | KEY_TAB)
#define KEY_SHIFT_ENTER           (MOD_SHIFT | KEY_ENTER)
#define KEY_SHIFT_HOME            (MOD_SHIFT | KEY_HOME)
#define KEY_SHIFT_UP              (MOD_SHIFT | KEY_UP)
#define KEY_SHIFT_DOWN            (MOD_SHIFT | KEY_DOWN)
#define KEY_SHIFT_LEFT            (MOD_SHIFT | KEY_LEFT)
#define KEY_SHIFT_RIGHT           (MOD_SHIFT | KEY_RIGHT)
#define KEY_SHIFT_HOME            (MOD_SHIFT | KEY_HOME)
#define KEY_SHIFT_END             (MOD_SHIFT | KEY_END)
#define KEY_SHIFT_PAGE_UP         (MOD_SHIFT | KEY_PAGE_UP)
#define KEY_SHIFT_PAGE_DOWN       (MOD_SHIFT | KEY_PAGE_DOWN)
#define KEY_SHIFT_INSERT          (MOD_SHIFT | KEY_INSERT)
#define KEY_SHIFT_DELETE          (MOD_SHIFT | KEY_DELETE)
#define KEY_SHIFT_BACKSPACE       (MOD_SHIFT | KEY_BACKSPACE)

#define KEY_CTRL_SLASH            (MOD_CTRL | KEY_SLASH)
#define KEY_CTRL_TAB              (MOD_CTRL | KEY_TAB)
#define KEY_CTRL_ENTER            (MOD_CTRL | KEY_ENTER)
#define KEY_CTRL_HOME             (MOD_CTRL | KEY_HOME)
#define KEY_CTRL_UP               (MOD_CTRL | KEY_UP)
#define KEY_CTRL_DOWN             (MOD_CTRL | KEY_DOWN)
#define KEY_CTRL_LEFT             (MOD_CTRL | KEY_LEFT)
#define KEY_CTRL_RIGHT            (MOD_CTRL | KEY_RIGHT)
#define KEY_CTRL_HOME             (MOD_CTRL | KEY_HOME)
#define KEY_CTRL_END              (MOD_CTRL | KEY_END)
#define KEY_CTRL_PAGE_UP          (MOD_CTRL | KEY_PAGE_UP)
#define KEY_CTRL_PAGE_DOWN        (MOD_CTRL | KEY_PAGE_DOWN)
#define KEY_CTRL_INSERT           (MOD_CTRL | KEY_INSERT)
#define KEY_CTRL_DELETE           (MOD_CTRL | KEY_DELETE)
#define KEY_CTRL_BACKSPACE        (MOD_CTRL | KEY_BACKSPACE)

#define KEY_ALT_SLASH             (MOD_ALT | KEY_SLASH)
#define KEY_ALT_TAB               (MOD_ALT | KEY_TAB)
#define KEY_ALT_ENTER             (MOD_ALT | KEY_ENTER)
#define KEY_ALT_HOME              (MOD_ALT | KEY_HOME)
#define KEY_ALT_UP                (MOD_ALT | KEY_UP)
#define KEY_ALT_DOWN              (MOD_ALT | KEY_DOWN)
#define KEY_ALT_LEFT              (MOD_ALT | KEY_LEFT)
#define KEY_ALT_RIGHT             (MOD_ALT | KEY_RIGHT)
#define KEY_ALT_HOME              (MOD_ALT | KEY_HOME)
#define KEY_ALT_END               (MOD_ALT | KEY_END)
#define KEY_ALT_PAGE_UP           (MOD_ALT | KEY_PAGE_UP)
#define KEY_ALT_PAGE_DOWN         (MOD_ALT | KEY_PAGE_DOWN)
#define KEY_ALT_INSERT            (MOD_ALT | KEY_INSERT)
#define KEY_ALT_DELETE            (MOD_ALT | KEY_DELETE)
#define KEY_ALT_BACKSPACE         (MOD_ALT | KEY_BACKSPACE)

#define KEY_CTRL_SHIFT_SLASH      (MOD_CTRL | MOD_SHIFT | KEY_SLASH)
#define KEY_CTRL_SHIFT_TAB        (MOD_CTRL | MOD_SHIFT | KEY_TAB)
#define KEY_CTRL_SHIFT_ENTER      (MOD_CTRL | MOD_SHIFT | KEY_ENTER)
#define KEY_CTRL_SHIFT_HOME       (MOD_CTRL | MOD_SHIFT | KEY_HOME)
#define KEY_CTRL_SHIFT_UP         (MOD_CTRL | MOD_SHIFT | KEY_UP)
#define KEY_CTRL_SHIFT_DOWN       (MOD_CTRL | MOD_SHIFT | KEY_DOWN)
#define KEY_CTRL_SHIFT_LEFT       (MOD_CTRL | MOD_SHIFT | KEY_LEFT)
#define KEY_CTRL_SHIFT_RIGHT      (MOD_CTRL | MOD_SHIFT | KEY_RIGHT)
#define KEY_CTRL_SHIFT_HOME       (MOD_CTRL | MOD_SHIFT | KEY_HOME)
#define KEY_CTRL_SHIFT_END        (MOD_CTRL | MOD_SHIFT | KEY_END)
#define KEY_CTRL_SHIFT_PAGE_UP    (MOD_CTRL | MOD_SHIFT | KEY_PAGE_UP)
#define KEY_CTRL_SHIFT_PAGE_DOWN  (MOD_CTRL | MOD_SHIFT | KEY_PAGE_DOWN)
#define KEY_CTRL_SHIFT_INSERT     (MOD_CTRL | MOD_SHIFT | KEY_INSERT)
#define KEY_CTRL_SHIFT_DELETE     (MOD_CTRL | MOD_SHIFT | KEY_DELETE)
#define KEY_CTRL_SHIFT_BACKSPACE  (MOD_CTRL | MOD_SHIFT | KEY_BACKSPACE)



typedef struct key_event {
    uint16_t keycode;
    uint8_t ascii;
} key_event_t;


#endif
