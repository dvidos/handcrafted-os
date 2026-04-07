#include "../libc_internal.h"
#include <ctype.h>  // For isspace, isdigit, toupper, isalpha
#include <limits.h> // For LLONG_MAX, LLONG_MIN
#include <errno.h>  // For ERANGE, EINVAL

long long strtoll(const char *str, char **endptr, int base) {
    const char *s = str;
    long long acc = 0;
    int sign = 1;
    long long cutoff;
    int cutlim;
    int any = 0; // Flag to indicate if any digits were processed
    unsigned char c;

    // 1. Skip leading whitespace
    while (isspace((unsigned char)*s)) {
        s++;
    }

    // 2. Handle optional sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // 3. Determine base if base is 0
    if (base == 0 || base == 16) { // Base 16 check for 0x prefix logic
        if (*s == '0') {
            s++;
            if (toupper((unsigned char)*s) == 'X' && s[0] != '\0') { // Check for "0x" or "0X"
                if (base == 0) base = 16;
                s++;
            } else if (base == 0) {
                base = 8;
            }
        } else if (base == 0) { // If it didn't start with '0', assume base 10
            base = 10;
        }
    }

    // Validate base
    if (base < 2 || base > 36) {
        if (endptr != NULL) {
            *endptr = (char *)str; // Point to original string if invalid base
        }
        errno = EINVAL;
        return 0LL;
    }

    // Set cutoff and cutlim for overflow checking
    if (sign == 1) {
        cutoff = LLONG_MAX / base;
        cutlim = LLONG_MAX % base;
    } else { // sign == -1
        cutoff = LLONG_MIN / base;
        cutlim = -(LLONG_MIN % base); // absolute value of remainder
    }

    // 4. Parse digits and letters according to the base
    for (c = (unsigned char)*s; ; c = (unsigned char)*++s) {
        int digit;

        if (isdigit(c)) {
            digit = c - '0';
        } else if (isalpha(c)) {
            digit = toupper(c) - 'A' + 10;
        } else {
            break; // Not a valid digit for the current base
        }

        if (digit >= base) {
            break; // Digit out of range for the base
        }

        // 5. Detect and handle overflow conditions
        if (sign == 1) { // Positive overflow
            if (acc > cutoff || (acc == cutoff && digit > cutlim)) {
                acc = LLONG_MAX;
                errno = ERANGE;
                any = 1; // Indicate that some conversion happened before overflow
                break;
            }
        } else { // Negative overflow/underflow
            if (acc < cutoff || (acc == cutoff && digit > cutlim)) {
                acc = LLONG_MIN;
                errno = ERANGE;
                any = 1; // Indicate that some conversion happened before underflow
                break;
            }
        }
        acc = acc * base + sign * digit;
        any = 1; // At least one digit processed
    }

    // 6. Manage endptr correctly
    if (endptr != NULL) {
        *endptr = (char *)(any ? s : str);
    }

    return acc;
}