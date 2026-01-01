#include "../fundamentals.h"
#include "keyboard_driver.h"
#include "../cpu/ports.h"
#include "../cpu/pic.h"
#include "../concepts/logger.h"
#include "../concepts/events.h"


// -- scan codes temp buffer ----------------------------------------

#define KBD_RING_BUFFER_SIZE 64

static volatile uint8_t mouse_ring_buffer[KBD_RING_BUFFER_SIZE];
static volatile uint8_t mouse_ring_head = 0;
static volatile uint8_t mouse_ring_tail = 0;

static inline void kbd_queue_push(uint8_t scancode) {
    uint8_t next = (mouse_ring_head + 1) % KBD_RING_BUFFER_SIZE;
    if (next != mouse_ring_tail) {
        mouse_ring_buffer[mouse_ring_head] = scancode;
        mouse_ring_head = next;
    }
}

static int kbd_queue_pop(uint8_t *scancode) {
    if (mouse_ring_tail == mouse_ring_head)
        return 0;

    *scancode = mouse_ring_buffer[mouse_ring_tail];
    mouse_ring_tail = (mouse_ring_tail + 1) % KBD_RING_BUFFER_SIZE;
    return 1;
}

// ----------------------------------------------

static int e0_prefix  = 0;
static keymods_t current_mods = MOD_NONE;

static const keycode_t scancode_to_keycode[128] = {
    // Numbers
    [0x02] = KEY_1, [0x03] = KEY_2, [0x04] = KEY_3, [0x05] = KEY_4,
    [0x06] = KEY_5, [0x07] = KEY_6, [0x08] = KEY_7, [0x09] = KEY_8,
    [0x0A] = KEY_9, [0x0B] = KEY_0,
    [0x0C] = KEY_MINUS, [0x0D] = KEY_EQUAL,

    // Letters
    [0x10] = KEY_Q, [0x11] = KEY_W, [0x12] = KEY_E, [0x13] = KEY_R,
    [0x14] = KEY_T, [0x15] = KEY_Y, [0x16] = KEY_U, [0x17] = KEY_I,
    [0x18] = KEY_O, [0x19] = KEY_P, [0x1E] = KEY_A, [0x1F] = KEY_S,
    [0x20] = KEY_D, [0x21] = KEY_F, [0x22] = KEY_G, [0x23] = KEY_H,
    [0x24] = KEY_J, [0x25] = KEY_K, [0x26] = KEY_L, [0x2C] = KEY_Z,
    [0x2D] = KEY_X, [0x2E] = KEY_C, [0x2F] = KEY_V, [0x30] = KEY_B,
    [0x31] = KEY_N, [0x32] = KEY_M,

    // Symbols / punctuation
    [0x1A] = KEY_LBRACKET, [0x1B] = KEY_RBRACKET,
    [0x2B] = KEY_BACKSLASH, [0x27] = KEY_APOSTROPHE, [0x28] = KEY_GRAVE,
    [0x33] = KEY_COMMA, [0x34] = KEY_DOT, [0x35] = KEY_SLASH,
    [0x39] = KEY_SPACE,

    // Control / navigation
    [0x1C] = KEY_ENTER, [0x01] = KEY_ESCAPE, [0x0E] = KEY_BACKSPACE,
    [0x0F] = KEY_TAB, [0x3A] = KEY_CAPSLOCK, [0x45] = KEY_NUMLOCK,
    [0x46] = KEY_SCROLLLOCK,

    // Shift / Ctrl / Alt / Super
    [0x2A] = KEY_SHIFT, // actually left shift
    [0x36] = KEY_SHIFT, // actually right shift
    [0x1D] = KEY_CTRL,
    [0x38] = KEY_ALT,
    [0x5B] = KEY_SUPER, // actually left super
    [0x5C] = KEY_SUPER, // actually right super

    // Function keys
    [0x3B] = KEY_F1, [0x3C] = KEY_F2, [0x3D] = KEY_F3, [0x3E] = KEY_F4,
    [0x3F] = KEY_F5, [0x40] = KEY_F6, [0x41] = KEY_F7, [0x42] = KEY_F8,
    [0x43] = KEY_F9, [0x44] = KEY_F10, [0x57] = KEY_F11, [0x58] = KEY_F12,
};

static const keycode_t e0_scancode_to_keycode[128] = {
    [0x1F] = KEY_CTRL, // actually Right Control
    [0x38] = KEY_ALT, // actually Right Alt
    [0x47] = KEY_HOME, [0x4F] = KEY_END,
    [0x49] = KEY_PAGEUP, [0x51] = KEY_PAGEDOWN,
    [0x52] = KEY_INSERT, [0x53] = KEY_DELETE,
    [0x48] = KEY_UP, [0x50] = KEY_DOWN,
    [0x4B] = KEY_LEFT, [0x4D] = KEY_RIGHT,
    [0x35] = KEY_KP_DIV, [0x37] = KEY_KP_MUL,
    [0x4A] = KEY_KP_MINUS, [0x4E] = KEY_KP_PLUS,
    [0x1C] = KEY_KP_ENTER,
    [0x52] = KEY_KP_0, [0x4F] = KEY_KP_1, [0x50] = KEY_KP_2, [0x51] = KEY_KP_3,
    [0x4B] = KEY_KP_4, [0x4C] = KEY_KP_5, [0x4D] = KEY_KP_6,
    [0x47] = KEY_KP_7, [0x48] = KEY_KP_8, [0x49] = KEY_KP_9,
    [0x53] = KEY_KP_DOT,
};

static void update_modifiers(keycode_t key, int pressed) {
    keymods_t mask = 0;

    if      (key == KEY_SHIFT) mask = MOD_SHIFT;
    else if (key == KEY_CTRL)  mask = MOD_CTRL;
    else if (key == KEY_ALT)   mask = MOD_ALT;
    else if (key == KEY_SUPER) mask = MOD_SUPER;

    current_mods = pressed ? (current_mods |= mask) : (current_mods &= ~mask);
}

char keycode_to_ascii(keycode_t keycode, int modifiers) {
    int shift = (modifiers & KEY_SHIFT) != 0;
    int ctrl  = (modifiers & KEY_CTRL) != 0;
    int alt   = (modifiers & KEY_ALT) != 0;

    // Letters
    if (keycode >= KEY_A && keycode <= KEY_Z) {
        char c = 'a' + (keycode - KEY_A);
        if (shift) c = 'A' + (keycode - KEY_A);
        // if (ctrl)  return 1 + (keycode - KEY_A); // Ctrl-A .. Ctrl-Z
        return c;
    }

    // Numbers and shifted symbols
    if (keycode >= KEY_1 && keycode <= KEY_0) {
        char c = "1234567890"[keycode - KEY_1];
        if (shift)
            c = "!@#$%^&*()"[keycode - KEY_1];
        return c;
    }

    switch (keycode) {
        // Space, Enter, Tab, Backspace
        case KEY_SPACE: return ' ';
        case KEY_ENTER: return '\n';
        case KEY_TAB:   return '\t';
        case KEY_BACKSPACE: return 0x08;
        // Symbols
        case KEY_MINUS:      return shift ? '_' : '-';
        case KEY_EQUAL:      return shift ? '+' : '=';
        case KEY_LBRACKET:   return shift ? '{' : '[';
        case KEY_RBRACKET:   return shift ? '}' : ']';
        case KEY_BACKSLASH:  return shift ? '|' : '\\';
        case KEY_SEMICOLON:  return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE:      return shift ? '~' : '`';
        case KEY_COMMA:      return shift ? '<' : ',';
        case KEY_DOT:        return shift ? '>' : '.';
        case KEY_SLASH:      return shift ? '?' : '/';
        default:
            return 0; // non-printable or unhandled
    }
}

void keyboard_driver_process() {
    uint8_t scancode;
    int pressed;

    while (kbd_queue_pop(&scancode)) {
        if (scancode == 0xE0) {
            e0_prefix = 1;
            continue;
        }

        pressed = (scancode & 0x80) == 0;
        scancode = scancode & 0x7F;

        keycode_t keycode = e0_prefix ? e0_scancode_to_keycode[scancode] : scancode_to_keycode[scancode];
        e0_prefix = 0; // reset before continuing

        if (keycode == KEY_CTRL || keycode == KEY_ALT || keycode == KEY_SHIFT || keycode == KEY_SUPER)
            update_modifiers(keycode, pressed);

        if (!pressed || !keycode)
            continue;
        
        key_event_t e;
        e.type = KEY_PRESSED;
        e.keycode = keycode;
        e.keymods = current_mods;
        e.ascii = keycode_to_ascii(keycode, current_mods);
        enqueue_key_event(&e);
        log.debug("Enqueing key event, code %d, mods %d, ascii %c", e.keycode, e.keymods, e.ascii);
    }
}

// -------------------------------------------------------------------

void keyboard_isr(void) {
    // keep it as small as possible, don't decode scancodes here
    uint8_t scancode = inb(0x60);
    //log.debug("keyboard_isr() scancode = 0x%02x", scancode);
    kbd_queue_push(scancode);
    pic_send_eoi(1);
}
