#include "libc_internal.h"

/**
 * @brief Checks if a character is an uppercase letter.
 *
 * This function determines if a given character `c` is an uppercase alphabetic
 * character (A-Z).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is an uppercase letter, 0 otherwise.
 */
int isupper(int c) {
    return (c >= 'A' && c <= 'Z');
}