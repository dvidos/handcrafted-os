#include "libc_internal.h"

/**
 * @brief Converts a lowercase letter to uppercase.
 *
 * If the given character `c` is a lowercase letter (a-z), this function
 * converts it to its corresponding uppercase letter. Otherwise, it returns
 * the character unchanged.
 *
 * @param c The character to convert, cast to an int.
 * @return The uppercase equivalent of `c` if `c` is a lowercase letter,
 *         otherwise `c` unchanged.
 */
int toupper(int c) {
    if (islower(c)) {
        return c - ('a' - 'A');
    }
    return c;
}