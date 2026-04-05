#include "libc_internal.h"

/**
 * @brief Computes the quotient and remainder of long long integer division.
 *
 * This function computes the quotient and remainder of the division of `numer`
 * by `denom`. The results are stored in an `lldiv_t` structure.
 *
 * @param numer The numerator.
 * @param denom The denominator.
 * @return An `lldiv_t` structure containing the quotient (`quot`) and remainder (`rem`).
 */
lldiv_t lldiv(long long numer, long long denom) {
    lldiv_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}