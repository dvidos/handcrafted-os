#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "screen.h"


int strlen(const char* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}

int strcmp(char *a, char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return (int)(*a - *b);
        }
        a++;
        b++;
    }
    // either one or both are '\0'
    return (int)(*a - *b);
}

void strcpy(char *target, char *source) {
    while (*source != '\0') {
        *target++ = *source++;
    }
    *target = *source; // final null char
}

void memset(void *dest, uint8_t value, size_t size) {
    char *d = (char *)dest;
    while (size-- > 0) {
        *d++ = value;
    }
}


void memcpy(void *dest, void *source, size_t size) {
    char *d = (char *)dest;
    char *s = (char *)source;
    while (size-- > 0) {
        *d++ = *s++;
    }
}

int memcmp(void *a, void *b, size_t size) {
    char *ca = (char *)a;
    char *cb = (char *)b;
    while (size-- > 0) {
        if (*ca != *cb)
            return (int)(*ca - *cb);
    }
    return 0;
}

char *memmove(void *dest, void *source, size_t size) {
    char *d = dest;
    char *s = source;

    if (dest > source && dest < (source + size)) {
        // if destination resides inside source, 
        // we must go from end to start
        d += size - 1;
        s += size - 1;
        while (size-- > 0) {
            *d-- = *s--;
        }
    } else {
        while (size-- > 0) {
            *d++ = *s++;
        }
    }

    return dest;
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

char *strchr(char *str, char c) {
    while (*str != '\0') {
        if (*str == c)
            return str;
        str++;
    }
    return NULL;
}

// first call str has to have a string, subsequent calls must pass NULL
char *strtok(char *str, char *delimiters) {
    // printf("strtok(\"%s\", \"%s\")\n", str, delimiters);
    static char *next = NULL;

    // if NULL is passed, remember last tokenized pointer
    if (str == NULL) {
        if (next == NULL) // user has not called us with valid string yet
            return NULL;
        str = next;
        // printf("strtok(): Continuing at... [%s]\n", str);
    }

    // skip over delimiters in the start of the string
    // printf("strtok(): Skipping over delimiters [%s]\n", delimiters);
    while (*str != '\0' && strchr(delimiters, *str) != NULL)
        str++;
    if (*str == '\0') {
        return NULL; // we reached the end
    }

    // find the next delimiter, we are on a non-delimiter character
    // printf("strtok(): Start of token is [%c]\n", *str);
    char *end = str;
    while (true) {
        if (*end == '\0') {
            next = NULL;
            return str;
        } else if (strchr(delimiters, *end) != NULL) {
            // we found a delimiter
            // printf("strtok(): Found delimiter [%c]\n", *end);
            *end = '\0';
            next = end + 1; // save for later
            return str;
        }
        end++;
    }
}

int atoi(char *str) {
    while (*str == ' ')
        str++;
    if (*str == '\0')
        return 0;

    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    }

    int radix;
    if (*str == '0' && *(str+1) == 'x') {
        radix = 16;
        str += 2;
    } else if (*str == '0') {
        radix = 8;
        str++;
    } else if (*str == 'b') {
        radix = 2;
        str++;
    } else {
        radix = 10;
    }

    int value = 0;
    while (*str != '\0') {
        int digit_value = 0;
        if (*str >= 'A' && *str <= 'F' && radix == 16) {
            digit_value = 10 + ((*str) - 'A');
        } else if (*str >= 'a' && *str <= 'f' && radix == 16) {
            digit_value = 10 + ((*str) - 'a');
        } else if (*str >= '0' && *str <= '9' && (radix == 10 || radix == 16)) {
            digit_value = (*str) - '0';
        } else if (*str >= '0' && *str <= '7' && radix == 8) {
            digit_value = (*str) - '0';
        } else if (*str >= '0' && *str <= '1' && radix == 2) {
            digit_value = (*str) - '0';
        } else {
            break;
        }
        value = value * radix + digit_value;
        str++;
    }

    if (negative)
        value = value * (-1);

    return value;
}
