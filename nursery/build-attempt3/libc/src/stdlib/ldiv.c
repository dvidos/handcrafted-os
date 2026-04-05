#include "libc_internal.h"

/**
 * @brief Computes the quotient and remainder of long integer division.
 *
 * This function computes the quotient and remainder of the division of `numer`
 * by `denom`. The results are stored in an `ldiv_t` structure.
 *
 * @param numer The numerator.
 * @param denom The denominator.
 * @return An `ldiv_t` structure containing the quotient (`quot`) and remainder (`rem`).
 */
ldiv_t ldiv(long numer, long denom) {
    ldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}