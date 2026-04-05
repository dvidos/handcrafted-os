#include "libc_internal.h"

/**
 * @brief Checks if a character is a hexadecimal digit.
 *
 * This function determines if a given character `c` is a hexadecimal digit
 * (0-9, a-f, A-F).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a hexadecimal digit, 0 otherwise.
 */
int isxdigit(int c) {
    return (isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}