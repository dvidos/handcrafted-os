#include "../libc_internal.h"

/**
 * @brief Checks if a character is a lowercase letter.
 *
 * This function determines if a given character `c` is a lowercase alphabetic
 * character (a-z).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a lowercase letter, 0 otherwise.
 */
int islower(int c) {
    return (c >= 'a' && c <= 'z');
}