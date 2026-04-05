#include "libc_internal.h"

/**
 * @brief Computes the difference between two calendar times.
 *
 * This function returns the difference between `time1` and `time0`,
 * expressed in seconds.
 *
 * @param time1 The first time_t value.
 * @param time0 The second time_t value.
 * @return The difference in seconds as a `double`.
 */
double difftime(time_t time1, time_t time0) {
    return (double)(time1 - time0);
}