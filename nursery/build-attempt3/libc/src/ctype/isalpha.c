#include "../libc_internal.h"

/**
 * @brief Checks if a character is alphabetic.
 *
 * This function determines if a given character `c` is an alphabetic character
 * (a-z or A-Z).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is an alphabetic character, 0 otherwise.
 */
int isalpha(int c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}