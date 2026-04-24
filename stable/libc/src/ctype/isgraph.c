#include "../libc_internal.h"

/**
 * @brief Checks if a character is a graphic character.
 *
 * This function determines if a given character `c` is a graphic character,
 * meaning it has a visible representation. Graphic characters are generally
 * any characters that are not control characters or spaces.
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a graphic character, 0 otherwise.
 */
int isgraph(int c) {
    return (c > 0x20 && c < 0x7F); // ASCII characters excluding space and control characters
}