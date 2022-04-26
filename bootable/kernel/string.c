#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


int strlen(const char* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}

void memset(char *dest, uint8_t value, size_t size) {
    while (size-- > 0) {
        *(dest++) = value;
    }
}


void memcpy(char *dest, char *source, size_t size) {
    while (size-- > 0) {
        *dest++ = *source++;
    }
}

void reverse(char *buffer, int len) {
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
