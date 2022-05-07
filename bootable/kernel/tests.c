#include "screen.h"
#include "string.h"


#define assert(a)    if (!(a)) { printf("Assertion failed! %s, file %s, line %u\n", #a, __FILE__, __LINE__); } else { printf("."); }



void run_tests() {
    char buffer[64];
    char *p;

    assert(strlen("") == 0);
    assert(strlen("Hello") == 5);

    assert(strcmp("", "") == 0);
    assert(strcmp("", "a") < 0);
    assert(strcmp("a", "") > 0);
    assert(strcmp("abc", "def") < 0);
    assert(strcmp("abc", "123") > 0);
    assert(strcmp("abc", "abc") == 0);
    
    buffer[0] = 0;
    strcpy(buffer, "123");
    assert(strcmp(buffer, "123") == 0);

    memset(buffer, 0xff, sizeof(buffer));
    strcpy(buffer + 1, "xyz");
    assert(memcmp(buffer, "\xffxyz\0\xff", 6) == 0);

    // strtok is used to parse commands in konsole
    strcpy(buffer, "Hello there, this is-a test... phrase!");
    p = " ,.!";
    assert(strcmp(strtok(buffer, p), "Hello") == 0);
    assert(strcmp(strtok(NULL, p), "there") == 0);
    assert(strcmp(strtok(NULL, p), "this") == 0);
    assert(strcmp(strtok(NULL, p), "is-a") == 0);
    assert(strcmp(strtok(NULL, p), "test") == 0);
    assert(strcmp(strtok(NULL, p), "phrase") == 0);
    assert(strtok(NULL, p) == NULL);
    // remember, strtok() alters the string in place
    assert(memcmp(buffer, "Hello\0there\0\0this\0is-a\0test\0\0\0\0phrase\0", 38) == 0);

    strcpy(buffer, "Hello there!");
    assert(strchr(buffer, 'o') == &buffer[4]);
    assert(strchr(buffer, 'T') == NULL);

    strcpy(buffer,        "0123456789abcdefghijklmnopqrstuvwxyz");
    memmove(&buffer[5], &buffer[10], 10);
    assert(strcmp(buffer, "01234abcdefghijfghijklmnopqrstuvwxyz") == 0);
    strcpy(buffer, "0123456789abcdefghijklmnopqrstuvwxyz");
    memmove(&buffer[10], &buffer[5], 10);
    assert(strcmp(buffer, "012345678956789abcdeklmnopqrstuvwxyz") == 0);

    // prove atoi()
    assert(atoi(NULL) == 0);
    assert(atoi("") == 0);
    assert(atoi("100") == 100);
    assert(atoi("-100") == -100);
    assert(atoi("0xff") == 255);
    assert(atoi("0xFF") == 255);
    assert(atoi("077") == 63);
    assert(atoi("b1001") == 9);

    // ltoa should work with ant 32bit signed value
    ltoa(0, buffer, 10);
    assert(strcmp(buffer, "0") == 0);
    ltoa(123, buffer, 10);
    assert(strcmp(buffer, "123") == 0);
    ltoa(-123, buffer, 10);
    assert(strcmp(buffer, "-123") == 0);
    ltoa(32, buffer, 16);
    assert(strcmp(buffer, "20") == 0);
    ltoa(32, buffer, 8);
    assert(strcmp(buffer, "40") == 0);
    ltoa(9, buffer, 2);
    assert(strcmp(buffer, "1001") == 0);

    // prove ltoa() overflows for values over half, and that ultoa() works
    ltoa(3000000000, buffer, 10);
    assert(strcmp(buffer, "-1294967296") == 0);
    ultoa(3000000000, buffer, 10);
    assert(strcmp(buffer, "3000000000") == 0);
    ultoa(4294967295, buffer, 10);
    assert(strcmp(buffer, "4294967295") == 0);
    ultoa(4294967295, buffer, 16);
    assert(strcmp(buffer, "FFFFFFFF") == 0);




}