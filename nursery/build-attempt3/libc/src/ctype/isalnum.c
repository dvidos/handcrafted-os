#include "libc_internal.h"

/**
 * @brief Checks if a character is alphanumeric.
 *
 * This function determines if a given character `c` is either an alphabetic character
 * (a-z, A-Z) or a digit (0-9).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is alphanumeric, 0 otherwise.
 */
int isalnum(int c) {
    return (isalpha(c) || isdigit(c));
}