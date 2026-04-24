#include "../libc_internal.h"
#include <ctype.h>  // For isspace, isdigit, toupper, isalpha
#include <limits.h> // For ULONG_MAX
#include <errno.h>  // For ERANGE, EINVAL

unsigned long strtoul(const char *str, char **endptr, int base) {
    const char *s = str;
    unsigned long acc = 0;
    unsigned long cutoff;
    int cutlim;
    int any = 0; // Flag to indicate if any digits were processed
    unsigned char c;

    // 1. Skip leading whitespace
    while (isspace((unsigned char)*s)) {
        s++;
    }

    // 2. Determine base if base is 0
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
        return 0UL;
    }

    // Set cutoff and cutlim for overflow checking
    cutoff = ULONG_MAX / base;
    cutlim = ULONG_MAX % base;

    // 3. Parse digits and letters according to the base
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

        // 4. Detect and handle overflow conditions
        if (acc > cutoff || (acc == cutoff && digit > cutlim)) {
            acc = ULONG_MAX;
            errno = ERANGE;
            any = 1; // Indicate that some conversion happened before overflow
            break;
        }

        acc = acc * base + digit;
        any = 1; // At least one digit processed
    }

    // 5. Manage endptr correctly
    if (endptr != NULL) {
        *endptr = (char *)(any ? s : str);
    }

    return acc;
}