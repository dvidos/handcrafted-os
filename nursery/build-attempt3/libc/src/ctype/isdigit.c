#include "libc_internal.h"

/**
 * @brief Checks if a character is a decimal digit.
 *
 * This function determines if a given character `c` is a decimal digit (0-9).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a digit, 0 otherwise.
 */
int isdigit(int c) {
    return (c >= '0' && c <= '9');
}