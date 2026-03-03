#include <string.h>


void ultoa(unsigned long num, char *buffer, int base) {

    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    int pos = 0;
    while (num > 0) {
        int remainder = num % base;
        buffer[pos++] = (remainder >= 10) ? ('A' + (remainder - 10)) : ('0' + remainder);
        num = num / base;
    }
    buffer[pos] = '\0';
    reverse(buffer, pos);
}

