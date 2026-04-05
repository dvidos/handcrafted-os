#include "libc_internal.h"

/**
 * @brief Checks if a character is a printable character.
 *
 * This function determines if a given character `c` is a printable character.
 * Printable characters include alphanumeric characters, punctuation, and space.
 * They typically have ASCII values 0x20 (space) through 0x7E (~).
 *
 * @param c The character to check, cast to an int.
 * @return Non-zero if `c` is a printable character, 0 otherwise.
 */
int isprint(int c) {
    return (c >= 0x20 && c < 0x7F); // ASCII space to tilde
}