#include <klib/string.h>
#include "framework.h"



void test_strings() {
    char buffer[64];
    char *str;

    // strlen
    assert(strlen("") == 0);
    assert(strlen("Hello") == 5);

    // strcmp
    assert(strcmp("", "") == 0);
    assert(strcmp("", "a") < 0);
    assert(strcmp("a", "") > 0);
    assert(strcmp("abc", "def") < 0);
    assert(strcmp("abc", "123") > 0);
    assert(strcmp("abc", "abc") == 0);
    
    // strcpy & strcmp ?
    buffer[0] = 0;
    strcpy(buffer, "123");
    assert(strcmp(buffer, "123") == 0);

    // memset & memcmp ?
    memset(buffer, 0xff, sizeof(buffer));
    strcpy(buffer + 1, "xyz");
    assert(memcmp(buffer, "\xffxyz\0\xff", 6) == 0);

    // strtok is used to parse commands in konsole
    strcpy(buffer, "Hello there, this is-a test... phrase!");
    str = " ,.!";
    assert(strcmp(strtok(buffer, str), "Hello") == 0);
    assert(strcmp(strtok(NULL, str), "there") == 0);
    assert(strcmp(strtok(NULL, str), "this") == 0);
    assert(strcmp(strtok(NULL, str), "is-a") == 0);
    assert(strcmp(strtok(NULL, str), "test") == 0);
    assert(strcmp(strtok(NULL, str), "phrase") == 0);
    assert(strtok(NULL, str) == NULL);
    // remember, strtok() alters the string in place nulling out all delimiters
    assert(memcmp(buffer, "Hello\0there\0\0this\0is-a\0test\0\0\0\0phrase\0", 38) == 0);

    // strchr()
    strcpy(buffer, "Hello there!");
    assert(strchr(buffer, 'o') == &buffer[4]);
    assert(strchr(buffer, 'T') == NULL);

    // memmove()
    strcpy(buffer,        "0123456789abcdefghijklmnopqrstuvwxyz");
    memmove(&buffer[5], &buffer[10], 10);
    assert(strcmp(buffer, "01234abcdefghijfghijklmnopqrstuvwxyz") == 0);
    strcpy(buffer,        "0123456789abcdefghijklmnopqrstuvwxyz");
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
    assert(atoi("2147483647") == 2147483647);  // max signed int
    assert(atoi("2147483649") == -2147483647); // wraparound

    // unsigned version
    assert(atoui("100") == 100);
    assert(atoui("-100") == 0); // minus is not recognized
    assert(atoui("2147483647") == 2147483647u);  // max signed int
    assert(atoui("2147483649") == 2147483649u);
    assert(atoui("4294967295") == 4294967295u);  // max unsigned int

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

    str = "once upon a time, a kernel was written";
    assert(strstr(str, NULL) == str);
    assert(strstr(str, "") == str);
    assert(strstr(str, "z") == NULL);
    assert(strstr(str, "o") == str);
    assert(strstr(str, "on a time") != NULL);
    assert(strstr(str, "on a time") == str + 7);
    assert(strstr(str, "on a time in the future") == NULL);
    assert(strstr(str, "written") != NULL);
    assert(strstr(str, "written too") == NULL);

    str = "writing tests";
    assert(memcmp(str, "", 0) == 0);
    assert(memcmp(str, "writ", 0) == 0);
    assert(memcmp(str, "writ", 4) == 0);
    assert(memcmp(str, "wri", 4) != 0); // zero terminator should not match
    assert(memcmp(str, "wrie", 4) != 0);
    assert(memcmp(str, "writing tests", strlen(str)) == 0);
    assert(memcmp(str, "writing tests again", strlen(str)) == 0);
    assert(memcmp(str, "writing tests again", strlen(str) + 1) != 0); // zero term

    str = "copying tests";
    memset(buffer, 0, sizeof(buffer));
    assert(buffer[0] == '\0');
    assert(buffer[sizeof(buffer) - 1] == '\0');
    memcpy(buffer, str, 0);
    assert(buffer[0] == '\0');

    memcpy(buffer + 1, str, 4);
    assert(memcmp(buffer, "\0copy\0", 6) == 0);
}

