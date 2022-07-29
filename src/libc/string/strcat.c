#include <string.h>

void strcat(char *target, char *src) {
    
    char *dest = target;
    while (*dest != '\0')
        dest++;
    
    while (*src != '\0') {
        *dest++ = *src++;
    }
    *dest = *src; // final null char
}

