#include <ctype.h>
#include <stddef.h> // For size_t, NULL
#include <stdbool.h> // For bool

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isalpha(int c) {
    return islower(c) || isupper(c);
}

int isblank(int c) {
    return c == ' ' || c == '\t';
}

int iscntrl(int c) {
    return (c >= 0 && c <= 0x1F) || c == 0x7F;
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isgraph(int c) {
    // TODO: Implement this function
    return 0;
}

int islower(int c) {
    // TODO: Implement this function
    return 0;
}

int isprint(int c) {
    return c >= 0x20 && c <= 0x7E;
}

int ispunct(int c) {
    return isgraph(c) && !isalnum(c);
}

int isspace(int c) {
    // TODO: Implement this function
    return 0;
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int tolower(int c) {
    if (isupper(c)) {
        return c - 'A' + 'a';
    }
    return c;
}

int toupper(int c) {
    if (islower(c)) {
        return c - 'a' + 'A';
    }
    return c;
}



