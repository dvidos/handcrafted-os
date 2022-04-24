#include <stdint.h>
#include <stdbool.h>
#include "screen.h"
#include "ports.h"
#include "idt.h"


bool extended = false;
bool left_ctrl = false;
bool right_ctrl = false;
bool left_shift = false;
bool right_shift = false;
bool left_alt = false;
bool right_alt = false;
bool caps_lock = false;
bool num_lock = false;


struct key_event {
    uint8_t ascii; // if printable (in the future, support for utf8)
    uint8_t non_printable; // e.g. arrows or multimedia
    uint8_t ctrl_down: 1;
    uint8_t alt_down: 1;
    uint8_t shift_down: 1;
} __attribute__((packed));

struct key_event events_queue[256];
uint8_t queue_head;
uint8_t queue_tail;

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


struct scancode_info {
    uint8_t printable;
    uint8_t shifted_printable;
    uint8_t special_key;
    uint8_t extended_key;
} __attribute__((packed));

struct scancode_info scancode_map[] = {
    /* 0x00 */ {0,   0,   0,              0},
    /* 0x01 */ {27,  27,  KEY_ESCAPE,     0}, // escape
    /* 0x02 */ {'1', '!', 0,              0},
    /* 0x03 */ {'2', '@', 0,              0},
    /* 0x04 */ {'3', '#', 0,              0},
    /* 0x05 */ {'4', '$', 0,              0},
    /* 0x06 */ {'5', '%', 0,              0},
    /* 0x07 */ {'6', '^', 0,              0},
    /* 0x08 */ {'7', '&', 0,              0},
    /* 0x09 */ {'8', '*', 0,              0},
    /* 0x0a */ {'9', '(', 0,              0},
    /* 0x0b */ {'0', ')', 0,              0},
    /* 0x0c */ {'-', '_', 0,              0},
    /* 0x0d */ {'=', '+', 0,              0},
    /* 0x0e */ {8,   8,   KEY_BACKSPACE,  0}, // bakspace
    /* 0x0f */ {9,   9,   KEY_TAB,        0}, // tab
    /* 0x10 */ {'q', 'Q', 0,              KEY_MEDIA_PREV},
    /* 0x11 */ {'w', 'W', 0,              0},
    /* 0x12 */ {'e', 'E', 0,              0},
    /* 0x13 */ {'r', 'R', 0,              0},
    /* 0x14 */ {'t', 'T', 0,              0},
    /* 0x15 */ {'y', 'Y', 0,              0},
    /* 0x16 */ {'u', 'U', 0,              0},
    /* 0x17 */ {'i', 'I', 0,              0},
    /* 0x18 */ {'o', 'O', 0,              0},
    /* 0x19 */ {'p', 'P', 0,              KEY_MEDIA_NEXT},
    /* 0x1a */ {'[', '{', 0,              0},
    /* 0x1b */ {']', '}', 0,              0},
    /* 0x1c */ {13,  13,  KEY_ENTER,      KEY_KP_ENTER}, // enter
    /* 0x1d */ {0,   0,   0,              0}, // left control
    /* 0x1e */ {'a', 'A', 0,              0},
    /* 0x1f */ {'s', 'S', 0,              0},
    /* 0x20 */ {'d', 'D', 0,              KEY_MEDIA_MUTE},
    /* 0x21 */ {'f', 'F', 0,              KEY_MEDIA_CALC},
    /* 0x22 */ {'g', 'G', 0,              KEY_MEDIA_PLAY},
    /* 0x23 */ {'h', 'H', 0,              0},
    /* 0x24 */ {'j', 'J', 0,              KEY_MEDIA_STOP},
    /* 0x25 */ {'k', 'K', 0,              0},
    /* 0x26 */ {'l', 'L', 0,              0},
    /* 0x27 */ {';', ':', 0,              0},
    /* 0x28 */ {'\'','"', 0,              0},
    /* 0x29 */ {'`', '~', 0,              0},
    /* 0x2a */ {0,   0,   0,              0}, // left shift
    /* 0x2b */ {'\\','|', 0,              0},
    /* 0x2c */ {'z', 'Z', 0,              0},
    /* 0x2d */ {'x', 'X', 0,              0},
    /* 0x2e */ {'c', 'C', 0,              KEY_MEDIA_VOL_DOWN},
    /* 0x2f */ {'v', 'V', 0,              0},
    /* 0x30 */ {'b', 'B', 0,              KEY_MEDIA_VOL_UP},
    /* 0x31 */ {'n', 'N', 0,              0},
    /* 0x32 */ {'m', 'M', 0,              KEY_MEDIA_WWW},
    /* 0x33 */ {',', '<', 0,              0},
    /* 0x34 */ {'.', '>', 0,              0},
    /* 0x35 */ {'/', '?', 0,              KEY_KP_SLASH},
    /* 0x36 */ {0,   0,   0,              0}, // right shift
    /* 0x37 */ {'*', '*', KEY_KP_STAR,    0}, // KP star
    /* 0x38 */ {0,   0,   0,              0}, // left alt
    /* 0x39 */ {' ', ' ', KEY_SPACE,      0}, // space
    /* 0x3a */ {0,   0,   0,              0}, // caps lock
    /* 0x3b */ {0,   0,   KEY_F1,         0}, // F1
    /* 0x3c */ {0,   0,   KEY_F2,         0}, // F2
    /* 0x3d */ {0,   0,   KEY_F3,         0}, // F3
    /* 0x3e */ {0,   0,   KEY_F4,         0}, // F4
    /* 0x3f */ {0,   0,   KEY_F5,         0}, // F5
    /* 0x40 */ {0,   0,   KEY_F6,         0}, // F6
    /* 0x41 */ {0,   0,   KEY_F7,         0}, // F7
    /* 0x42 */ {0,   0,   KEY_F8,         0}, // F8
    /* 0x43 */ {0,   0,   KEY_F9,         0}, // F9
    /* 0x44 */ {0,   0,   KEY_F10,        0}, // F10
    /* 0x45 */ {0,   0,   0,              0}, // num lock
    /* 0x46 */ {0,   0,   0,              0}, // scroll lock
    /* 0x47 */ {0,   '7', KEY_KP_7,       KEY_HOME},// key pad 7
    /* 0x48 */ {0,   '8', KEY_KP_8,       KEY_UP},// key pad 8
    /* 0x49 */ {0,   '9', KEY_KP_9,       KEY_PAGE_UP},// key pad 9
    /* 0x4a */ {'-', '-', KEY_KP_MINUS,   0}, // key pad minus
    /* 0x4b */ {0,   '4', KEY_KP_4,       KEY_LEFT},// key pad 4
    /* 0x4c */ {0,   '5', KEY_KP_5,       0},// key pad 5
    /* 0x4d */ {0,   '6', KEY_KP_6,       KEY_RIGHT},// key pad 6
    /* 0x4e */ {'+', '+', KEY_KP_PLUS,    0}, // key pad plus
    /* 0x4f */ {0,   '1', KEY_KP_1,       KEY_END},// key pad 1
    /* 0x50 */ {0,   '2', KEY_KP_2,       KEY_DOWN},// key pad 2
    /* 0x51 */ {0,   '3', KEY_KP_3,       KEY_PAGE_DOWN},// key pad 3
    /* 0x52 */ {0,   '0', KEY_KP_0,       KEY_INSERT},// key pad 3
    /* 0x53 */ {0,   '.', KEY_KP_DECIMAL, KEY_DELETE}, // key pad decimal point,
    /* 0x54 */ {0,   0,   0,              0}, // empty 1
    /* 0x55 */ {0,   0,   0,              0}, // empty 2
    /* 0x56 */ {0,   0,   0,              0}, // empty 3
    /* 0x57 */ {0,   0,   KEY_F11,        0}, // F11
    /* 0x58 */ {0,   0,   KEY_F12,        0}, // F12

    // there are more expanded keys, mostly multimedia, if we want to
    // see https://wiki.osdev.org/PS2_Keyboard
};


void init_keyboard() {
    // not much...
}

void keyboard_handler(registers_t* regs) {
    (void)regs;

    uint8_t scancode = port_byte_in(0x60);
    if (scancode == 0xE0) {
        extended = true;
        // we'll get the next value in a subsequent interrupt
        return;
    }

    // if (extended) {
    //     printf("(extended)");
    // }
    // printf("0x%x ", scancode);

    bool down = true;
    if (scancode & 0x80) {
        down = false;
        scancode = scancode & 0x7F;
    }

    if (!extended) {
        switch (scancode) {
            case 0x1D:
                left_ctrl = down;
                break;
            case 0x2A:
                left_shift = down;
                break;
            case 0x36:
                right_shift = down;
                break;
            case 0x38:
                right_alt = down;
                break;
            case 0x3A:
                if (down)
                    caps_lock = !caps_lock;
                break;
            case 0x45:
                if (down)
                    num_lock = !num_lock;
                break;
        }
    } else { // extended
        switch (scancode) {
            case 0x1D:
                right_ctrl = down;
                break;
            case 0x38:
                right_alt = down;
                break;
        }
    }

    // from here on, we care only about down presses
    if (!down)
        return;

    // derive shifted conditions
    bool shifted = false;
    if (scancode >= 0x47 && scancode <= 0x53) {
        shifted = left_shift || right_shift || num_lock;  // numpad
    } else if ((scancode >= 0x10 && scancode <= 0x19)
            || (scancode >= 0x1e && scancode <= 0x26)
            || (scancode >= 0x2c && scancode <= 0x32)) {
        shifted = left_shift || right_shift || caps_lock; // normal letters
    } else {
        shifted = left_shift || right_shift; // all other symbols
    }

    // prepare the event data
    uint8_t printable = 0;
    uint8_t special_key = 0;

    if (!extended) {
        printable = shifted
            ? scancode_map[scancode].shifted_printable
            : scancode_map[scancode].printable;
        special_key = scancode_map[scancode].special_key;
    } else {
        printable = 0; // KEY_KP_SLASH will fail here
        special_key = scancode_map[scancode].extended_key;
    }

    if (printable != 0 || special_key != 0) {
        // add the key down event
    }

    printf("printable: [%c], special key: %d\n", printable, special_key);
    // clear this flag now
    extended = false;
}
