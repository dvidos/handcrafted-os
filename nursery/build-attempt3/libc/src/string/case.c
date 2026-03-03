#include <string.h>


int tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c | 0x20) : c;
}

int toupper(int c) {
    return (c >= 'a' && c <= 'z') ? (c & ~(int)0x20) : c;
}

