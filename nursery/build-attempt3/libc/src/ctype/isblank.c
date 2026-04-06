#include "../libc_internal.h"

/**
 * @brief Checks if a character is a blank character.
 *
 * This function determines if a given character `c` is a blank character,
 * typically a space (' ') or a horizontal tab ('	').
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a blank character, 0 otherwise.
 */
int isblank(int c) {
    return (c == ' ' || c == '	');
}