#include "libc_internal.h"

/**
 * @brief Computes the quotient and remainder of integer division.
 *
 * This function computes the quotient and remainder of the division of `numer`
 * by `denom`. The results are stored in a `div_t` structure.
 *
 * @param numer The numerator.
 * @param denom The denominator.
 * @return A `div_t` structure containing the quotient (`quot`) and remainder (`rem`).
 */
div_t div(int numer, int denom) {
    div_t result;
    result.quot = numer / denom;
    result.rem = numer % denom;
    return result;
}