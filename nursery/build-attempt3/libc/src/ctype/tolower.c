#include "../libc_internal.h"

/**
 * @brief Converts an uppercase letter to lowercase.
 *
 * If the given character `c` is an uppercase letter (A-Z), this function
 * converts it to its corresponding lowercase letter. Otherwise, it returns
 * the character unchanged.
 *
 * @param c The character to convert, cast to an int.
 * @return The lowercase equivalent of `c` if `c` is an uppercase letter,
 *         otherwise `c` unchanged.
 */
int tolower(int c) {
    if (isupper(c)) {
        return c + ('a' - 'A');
    }
    return c;
}