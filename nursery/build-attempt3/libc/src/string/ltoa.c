#include <string.h>


void ltoa(long num, char *buffer, int base) {

    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    bool negative = false;
    if (base == 10 && num < 0) {
        negative = true;
        num = num * -1;
    }

    int pos = 0;
    while (num > 0) {
        int remainder = num % base;
        buffer[pos++] = (remainder >= 10) ? ('A' + (remainder - 10)) : ('0' + remainder);
        num = num / base;
    }
    if (negative) {
        buffer[pos++] = '-';
    }
    buffer[pos] = '\0';
    reverse(buffer, pos);
}

