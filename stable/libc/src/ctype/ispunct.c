#include "../libc_internal.h"

/**
 * @brief Checks if a character is a punctuation character.
 *
 * This function determines if a given character `c` is a punctuation character.
 * Punctuation characters are graphic characters that are not alphanumeric.
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a punctuation character, 0 otherwise.
 */
int ispunct(int c) {
    return (isgraph(c) && !isalnum(c));
}