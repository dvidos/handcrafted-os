#include <string.h>


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
    if (*str == 'x') {
        radix = 16;
        str++;
    } else if (*str == '0' && *(str+1) == 'x') {
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

