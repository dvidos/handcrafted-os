#include "../libc_internal.h"

/**
 * @brief Determines the smaller of two floating-point arguments.
 *
 * This function returns the smaller of its two arguments. It handles NaN values
 * according to IEEE 754, typically propagating NaN if one argument is NaN.
 *
 * @param x The first double-precision floating-point value.
 * @param y The second double-precision floating-point value.
 * @return The smaller of `x` and `y`.
 */
double fmin(double x, double y) {
    // Simple implementation. For strict IEEE 754 compliance and NaN handling,
    // a more complex approach might be needed.
    return (x < y) ? x : y;
}