#include <ctypes.h>
#include <bits.h>

// how about endianness?
typedef uint16_t ucs2char;

int ucs2len(ucs2char *str) {
    int len = 0;

    // advances 2 bytes each time
    while (*str++ != 0)
        len++;

    return len;
}

void ucs2cpy(ucs2char *dst, ucs2char *src, int num_chars) {
    while (num_chars-- > 0) {
        *dst++ = *src++;
    }
    *dst = 0;
}

void ucs2set(ucs2char *str, ucs2char c, int num_chars) {
    while (num_chars-- > 0)
        *str++ = c;
}

void ucs2toa(ucs2char *ucs2, uint8_t *ascii) {
    while (*ucs2 != 0)
        *ascii++ = LOW_BYTE(*ucs2++);
    *ascii = 0;
}

void atoucs2(uint8_t *ascii, ucs2char *ucs2) {
    while (*ascii != 0)
        *ucs2++ = (ucs2char)*ascii++;
    *ucs2 = 0;
}
