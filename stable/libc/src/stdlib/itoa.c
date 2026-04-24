#include "../libc_internal.h"


static char *__int_to_str(unsigned long long value, char *str, int base, bool is_signed) {
    char *p = str;
    unsigned long long work_val = value;
    bool negative = false;

    // 1. handle the sign
    if (is_signed && (long long)value < 0) {
        negative = true;
        work_val = -(long long)value;
    }

    // 2. conversion Loop (generates digits in reverse order)
    do {
        uint32_t remainder;
        
        // if the value fits in 32 bits, use standard math
        if (work_val <= 0xFFFFFFFF) {
            remainder = (uint32_t)work_val % (uint32_t)base;
            work_val = (uint32_t)work_val / (uint32_t)base;

        } else {
            // manual 64-bit / 32-bit division to avoid libgcc dependency
            remainder = 0;
            unsigned long long quotient = 0;
            for (int i = 63; i >= 0; i--) {
                remainder = (remainder << 1) | ((work_val >> i) & 1);
                if (remainder >= (uint32_t)base) {
                    remainder -= (uint32_t)base;
                    quotient |= (1ULL << i);
                }
            }
            work_val = quotient;
        }

        *p++ = (remainder < 10) ? (remainder + '0') : (remainder - 10 + 'a');
    } while (work_val > 0);

    if (negative)
        *p++ = '-';
    *p = '\0';


    // 3. Reverse the string in place
    char *start = str;
    char *end = p - 1;
    while (start < end) {
        char swap = *start;
        *start++ = *end;
        *end-- = swap;
    }

    return str;
}

char *itoa(int value, char *str, int base) {
    return __int_to_str((unsigned long long)value, str, base, true);
}

char *ltoa(long value, char *str, int base) {
    return __int_to_str((unsigned long long)value, str, base, true);
}

char *ultoa(unsigned long value, char *str, int base) {
    return __int_to_str((unsigned long long)value, str, base, false);
}

char *lltoa(long long value, char *str, int base) {
    return __int_to_str((unsigned long long)value, str, base, true);
}

char *ulltoa(unsigned long long value, char *str, int base) {
    return __int_to_str(value, str, base, false);
}
