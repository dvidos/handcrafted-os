#include "libc_internal.h"

/**
 * @brief Checks if a character is a whitespace character.
 *
 * This function determines if a given character `c` is a standard whitespace
 * character. This typically includes space, form feed, new-line,
 * carriage return, horizontal tab, and vertical tab.
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a whitespace character, 0 otherwise.
 */
int isspace(int c) {
    return (c == ' ' || (c >= '	' && c <= ''));
}