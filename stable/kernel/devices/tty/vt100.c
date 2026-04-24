#include "../../include/uapi/base.h"
#include "../klib/string.h"
#include "../memory/kheap.h"
#include "vt100.h"
#include "vconsole.h"

// ASCII control codes
#define VT100_ESC 0x1B
#define SPACES_80 "                                                                                "



// Helper to reset CSI parameters
static void vt100_reset_params(vt100_t *vt) {
    memset(vt->params, 0, sizeof(vt->params));
    vt->param_idx = 0;
    memset(vt->int_params, 0, sizeof(vt->int_params));
    vt->int_param_count = 0;
}

// Helper to parse CSI parameters (e.g., "3;40;1")
static void vt100_parse_csi_params(vt100_t *vt) {
    if (vt->param_idx == 0) { // No parameters, means default
        vt->int_params[0] = 0; // Default for many commands is 0 or 1
        vt->int_param_count = 1;
        return;
    }

    char *start = vt->params;
    char *end = vt->params;
    vt->int_param_count = 0;

    while (*end != '\0' && vt->int_param_count < 4) {
        if (*end == ';') {
            *end = '\0'; // Null-terminate the current parameter string
            vt->int_params[vt->int_param_count++] = atoi(start);
            start = end + 1;
        }
        end++;
    }
    // Parse the last parameter
    if (*start != '\0' && vt->int_param_count < 4) {
        vt->int_params[vt->int_param_count++] = atoi(start);
    }

    // If no explicit parameters were parsed but there was input, set a default
    if (vt->int_param_count == 0 && vt->param_idx > 0) {
        vt->int_params[0] = 0; // Common default for many CSI sequences
        vt->int_param_count = 1;
    }
}


static void vt100_handle_csi_sequence(vt100_t *vt, char final_char) {
    vt100_parse_csi_params(vt);

    int p1 = (vt->int_param_count > 0) ? vt->int_params[0] : 0;
    int p2 = (vt->int_param_count > 1) ? vt->int_params[1] : 0;
    // int p3 = (vt->int_param_count > 2) ? vt->int_params[2] : 0; // Not used in common basic VT100 ops

    int current_row, current_col;
    int screen_rows, screen_cols;
    vt->vcons->ops->get_pos(vt->vcons, &current_row, &current_col);
    vt->vcons->ops->get_size(vt->vcons, &screen_rows, &screen_cols);

    switch (final_char) {
        case 'H': // Cursor Position (CUP) - ESC[row;colH
        case 'f': // Horizontal and Vertical Position (HVP) - ESC[row;colf
            // VT100 is 1-indexed for rows and cols, our vcons is 0-indexed
            vt->vcons->ops->set_pos(vt->vcons, (p1 == 0 ? 0 : p1 - 1), (p2 == 0 ? 0 : p2 - 1));
            break;

        case 'J': // Erase in Display (ED) - ESC[paramJ
            if (p1 == 0) { // Erase from cursor to end of vcons
                vt->vcons->ops->set_pos(vt->vcons, current_row, current_col);
                for (int r = current_row; r < screen_rows; r++) {
                    vt->vcons->ops->set_pos(vt->vcons, r, 0); // Move to start of line to clear
                    vt->vcons->ops->write(vt->vcons, SPACES_80, screen_cols); // Clear line with spaces
                }
                vt->vcons->ops->set_pos(vt->vcons, current_row, current_col); // Restore cursor
            } else if (p1 == 1) { // Erase from start of vcons to cursor
                vt->vcons->ops->set_pos(vt->vcons, 0, 0);
                for (int r = 0; r <= current_row; r++) {
                    vt->vcons->ops->write(vt->vcons, SPACES_80, screen_cols);
                }
                vt->vcons->ops->set_pos(vt->vcons, current_row, current_col);
            } else if (p1 == 2) { // Erase entire vcons
                vt->vcons->ops->clear(vt->vcons);
            }
            break;

        case 'K': // Erase in Line (EL) - ESC[paramK
            vt->vcons->ops->get_pos(vt->vcons, &current_row, &current_col);
            if (p1 == 0) { // Erase from cursor to end of line
                vt->vcons->ops->set_pos(vt->vcons, current_row, current_col);
                vt->vcons->ops->write(vt->vcons, SPACES_80, screen_cols - current_col);
                vt->vcons->ops->set_pos(vt->vcons, current_row, current_col); // Restore cursor
            } else if (p1 == 1) { // Erase from start of line to cursor
                vt->vcons->ops->set_pos(vt->vcons, current_row, 0);
                vt->vcons->ops->write(vt->vcons, SPACES_80, current_col + 1);
                vt->vcons->ops->set_pos(vt->vcons, current_row, current_col);
            } else if (p1 == 2) { // Erase entire line
                vt->vcons->ops->set_pos(vt->vcons, current_row, 0);
                vt->vcons->ops->write(vt->vcons, SPACES_80, screen_cols);
                vt->vcons->ops->set_pos(vt->vcons, current_row, current_col);
            }
            break;
        
        case 'm': // Select Graphic Rendition (SGR) - ESC[param;...m
            // Iterate through parameters to set attributes
            if (vt->int_param_count == 0) { // Default: reset all attributes
                vt->vcons->ops->set_text_attr(vt->vcons, vt->default_attr);
            } else {
                uint8_t current_attr;
                vt->vcons->ops->get_text_attr(vt->vcons, &current_attr);

                for (int i = 0; i < vt->int_param_count; i++) {
                    int param = vt->int_params[i];
                    switch (param) {
                        case 0: // Reset all attributes
                            current_attr = vt->default_attr;
                            break;
                        case 1: // Bold/Bright
                            current_attr = vconsole_fg_color(current_attr, current_attr & 0x0F, true);
                            break;
                        // Foreground colors
                        case 30: current_attr = vconsole_fg_color(current_attr, BLACK, false); break;
                        case 31: current_attr = vconsole_fg_color(current_attr, RED, false); break;
                        case 32: current_attr = vconsole_fg_color(current_attr, GREEN, false); break;
                        case 33: current_attr = vconsole_fg_color(current_attr, YELLOW, false); break;
                        case 34: current_attr = vconsole_fg_color(current_attr, BLUE, false); break;
                        case 35: current_attr = vconsole_fg_color(current_attr, MAGENTA, false); break;
                        case 36: current_attr = vconsole_fg_color(current_attr, CYAN, false); break;
                        case 37: current_attr = vconsole_fg_color(current_attr, WHITE, false); break;
                        // Background colors
                        case 40: current_attr = vconsole_bg_color(current_attr, BLACK); break;
                        case 41: current_attr = vconsole_bg_color(current_attr, RED); break;
                        case 42: current_attr = vconsole_bg_color(current_attr, GREEN); break;
                        case 43: current_attr = vconsole_bg_color(current_attr, YELLOW); break;
                        case 44: current_attr = vconsole_bg_color(current_attr, BLUE); break;
                        case 45: current_attr = vconsole_bg_color(current_attr, MAGENTA); break;
                        case 46: current_attr = vconsole_bg_color(current_attr, CYAN); break;
                        case 47: current_attr = vconsole_bg_color(current_attr, WHITE); break;
                        // TODO: Implement more SGR parameters as needed
                    }
                }
                vt->vcons->ops->set_text_attr(vt->vcons, current_attr);
            }
            break;

        case 's': // Save Cursor Position (SCP) - VT100 specific
            vt->vcons->ops->get_pos(vt->vcons, &vt->saved_row, &vt->saved_col);
            break;
        case 'u': // Restore Cursor Position (RCP) - VT100 specific
            vt->vcons->ops->set_pos(vt->vcons, vt->saved_row, vt->saved_col);
            break;

        // TODO: Add more CSI sequences as needed (e.g., cursor movement, scrolling)
        default:
            // Unhandled CSI sequence
            break;
    }
    vt100_reset_params(vt);
}

// Function to process incoming character data
void vt100_write(vt100_t *vt, char c) {
    if (!vt || !vt->vcons) return;

    switch (vt->state) {
        case VT100_STATE_NORMAL:
            if (c == VT100_ESC) {
                vt->state = VT100_STATE_ESCAPE;
            } else {
                vt->vcons->ops->putc(vt->vcons, c);
            }
            break;

        case VT100_STATE_ESCAPE:
            if (c == '[') {
                vt->state = VT100_STATE_CSI;
                vt100_reset_params(vt); // Prepare for CSI parameters
            } else if (c == 'E') { // NEL - Next Line
                int r, co;
                vt->vcons->ops->get_pos(vt->vcons, &r, &co);
                vt->vcons->ops->set_pos(vt->vcons, r + 1, 0);
                vt->state = VT100_STATE_NORMAL;
            } else if (c == 'M') { // RI - Reverse Index (scrolls up)
                // TODO: Implement reverse index / scroll down logic if needed
                // For now, it might involve moving content down and clearing top line
                vt->state = VT100_STATE_NORMAL;
            } else if (c == 'D') { // IND - Index (line feed without CR)
                int r, co;
                vt->vcons->ops->get_pos(vt->vcons, &r, &co);
                vt->vcons->ops->set_pos(vt->vcons, r + 1, co);
                vt->state = VT100_STATE_NORMAL;
            } else if (c == 's') { // Save Cursor Position - non-CSI VT100
                vt->vcons->ops->get_pos(vt->vcons, &vt->saved_row, &vt->saved_col);
                vt->state = VT100_STATE_NORMAL;
            } else if (c == 'u') { // Restore Cursor Position - non-CSI VT100
                vt->vcons->ops->set_pos(vt->vcons, vt->saved_row, vt->saved_col);
                vt->state = VT100_STATE_NORMAL;
            } else {
                // Unhandled escape sequence, revert to normal
                vt->state = VT100_STATE_NORMAL;
            }
            break;

        case VT100_STATE_CSI:
            if ((c >= '0' && c <= '9') || c == ';') { // Parameter characters
                if (vt->param_idx < (int)sizeof(vt->params) - 1) {
                    vt->params[vt->param_idx++] = c;
                    vt->params[vt->param_idx] = '\0'; // Always null-terminate
                }
            } else { // Final character of CSI sequence
                vt100_handle_csi_sequence(vt, c);
                vt->state = VT100_STATE_NORMAL;
            }
            break;
    }
}

// Function to process incoming string data
void vt100_puts(vt100_t *vt, const char *str) {
    if (!vt || !str) return;
    for (int i = 0; str[i] != '\0'; i++) {
        vt100_write(vt, str[i]);
    }
}

// Function to create and initialize a VT100 emulator instance
vt100_t *create_vt100(vconsole_t *vcons) {
    if (!vcons) return NULL;

    vt100_t *vt = (vt100_t *)kmalloc(sizeof(vt100_t));
    if (!vt) return NULL;

    memset(vt, 0, sizeof(vt100_t));
    vt->vcons = vcons;
    vt->state = VT100_STATE_NORMAL;
    vt->default_attr = vconsole_color(WHITE, BLACK, false, false);
    
    // Get initial cursor position and save it
    vt->vcons->ops->get_pos(vt->vcons, &vt->saved_row, &vt->saved_col);

    return vt;
}

// Function to destroy a VT100 emulator instance
void destroy_vt100(vt100_t *vt) {
    if (vt) {
        kfree(vt);
    }
}
