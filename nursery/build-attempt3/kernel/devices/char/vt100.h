#ifndef __KERNEL_DEVICES_CHAR_VT100_H__
#define __KERNEL_DEVICES_CHAR_VT100_H__

#include "text_screen.h"

// States for the VT100 parser state machine
typedef enum {
    VT100_STATE_NORMAL,
    VT100_STATE_ESCAPE,         // After receiving ESC
    VT100_STATE_CSI,            // After receiving ESC [
    // Add more states for other sequence types if needed, e.g., DCS, OSC, etc.
} vt100_state_t;

// Structure to hold VT100 emulator state
typedef struct vt100 {
    text_screen_t *screen;      // Dependency: the text screen to draw on

    vt100_state_t state;        // Current state of the parser
    char params[16];            // Buffer for CSI parameters (e.g., "3;4" in ESC[3;4H)
    int param_idx;              // Current index in params buffer
    int int_params[4];          // Parsed integer parameters
    int int_param_count;        // Number of parsed integer parameters

    // Saved cursor position (for ESC[s and ESC[u)
    int saved_row;
    int saved_col;

    // Default text attributes
    uint8_t default_attr;
} vt100_t;

// Function to create and initialize a VT100 emulator instance
vt100_t *create_vt100(text_screen_t *screen);

// Function to destroy a VT100 emulator instance
void destroy_vt100(vt100_t *vt);

// Function to process incoming character data
void vt100_write(vt100_t *vt, char c);

// Function to process incoming string data
void vt100_puts(vt100_t *vt, const char *str);

#endif // __KERNEL_DEVICES_CHAR_VT100_H__
