#include "types.h"
#include "funcs.h"


static void reverse(char *buffer, int len) {
    // reverse the buffer in place
    int left = 0;
    int right = len - 1;
    char c;
    while (left < right) {
        c = buffer[left];
        buffer[left] = buffer[right];
        buffer[right] = c;
        left++;
        right--;
    }
}

void itoa(int num, char *buffer, int base) {
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

void utoa(uint32 num, char *buffer, int base) {
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

uint32 strlen(char *str) {
    uint32 len = 0;
    while (*str != 0) { len++; str++; }
    return len;
}

void strcpy(char *target, const char *source) {
    while (*source != '\0') { *target++ = *source++; }
    *target = *source; // final null char
}

void strcat(char *target, char *source) {
    while (*target != '\0') { target++; }
    while (*source != '\0') { *target++ = *source++; }
    *target = *source; // final null char
}

void memset(char *target, char c, uint32 size) {
    while (size-- > 0) *target++ = c;
}

void memcpy(char* dest, char *source, uint32 size) {
    while (size-- > 0) *dest++ = *source++;
}

int memcmp(char *a, char *b, uint32 size) {
    while (size--) {
        if (*a < *b)
            return -1;
        if (*a > *b)
            return 1;
        a++;
        b++;
    }
    return 0;
}


void itosize(uint32 value, char *buffer) {
    // e.g. convert 1024 into "1K" etc
    if (value < KB) {
        itoa(value, buffer, 10);
    } else if (value < MB) {
        itoa(value / KB, buffer, 10);
        strcat(buffer, "K");
    } else if (value < GB) {
        itoa(value / MB, buffer, 10);
        strcat(buffer, "M");
    } else {
        itoa(value / GB, buffer, 10);
        strcat(buffer, "G");
    }
}

