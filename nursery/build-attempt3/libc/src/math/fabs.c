#include "libc_internal.h"

/**
 * @brief Computes the absolute value of a floating-point number.
 *
 * This function returns the absolute value of the double-precision floating-point
 * number `x`.
 *
 * @param x The double-precision floating-point value.
 * @return The absolute value of `x`.
 */
double fabs(double x) {
    // A simple implementation for demonstration. A more robust implementation
    // might consider NaN and infinity carefully depending on IEEE 754 compliance needs.
    return (x < 0.0) ? -x : x;
}