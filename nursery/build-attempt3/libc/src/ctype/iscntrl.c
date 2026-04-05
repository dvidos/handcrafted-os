#include "libc_internal.h"

/**
 * @brief Checks if a character is a control character.
 *
 * This function determines if a given character `c` is a control character.
 * Control characters typically have ASCII values 0x00-0x1F and 0x7F (DEL).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a control character, 0 otherwise.
 */
int iscntrl(int c) {
    return ((c >= 0 && c <= 0x1F) || (c == 0x7F));
}