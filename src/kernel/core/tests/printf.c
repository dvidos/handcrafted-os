#include <klib/string.h>
#include "framework.h"


void test_printf() {
    char buffer[64];

    sprintfn(buffer, sizeof(buffer), "");
    assert(strlen(buffer) == 0);

    sprintfn(buffer, sizeof(buffer), "Hello");
    assert(strcmp(buffer, "Hello") == 0);

    // respect buffer size (include zero terminator)

    sprintfn(buffer, 5, "aaaaaaaaaaaaaaa");
    assert(strlen(buffer) == 4);

    sprintfn(buffer, 5, "%s", "aaaaaaaaaaaaaaa");
    assert(strlen(buffer) == 4);

    // escape percent sign

    sprintfn(buffer, sizeof(buffer), "Hello 100%%");
    assert(strcmp(buffer, "Hello 100%") == 0);

    // chars

    sprintfn(buffer, sizeof(buffer), "%c+%c", 'a', 'b');
    assert(strcmp(buffer, "a+b") == 0);

    // basic strings 

    sprintfn(buffer, sizeof(buffer), "%s", "Hello");
    assert(strcmp(buffer, "Hello") == 0);

    sprintfn(buffer, sizeof(buffer), "%10s", "Hello");
    assert(strcmp(buffer, "     Hello") == 0);

    sprintfn(buffer, sizeof(buffer), "%-10s", "Hello");
    assert(strcmp(buffer, "Hello     ") == 0);

    // basic numbers

    sprintfn(buffer, sizeof(buffer), "%d", 0);
    assert(strcmp(buffer, "0") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", 5);
    assert(strcmp(buffer, "5") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", -5);
    assert(strcmp(buffer, "-5") == 0);

    // padding 

    sprintfn(buffer, sizeof(buffer), "%3d", 5);
    assert(strcmp(buffer, "  5") == 0);

    sprintfn(buffer, sizeof(buffer), "%-3d", 5);
    assert(strcmp(buffer, "5  ") == 0);

    sprintfn(buffer, sizeof(buffer), "%03d", 5);
    assert(strcmp(buffer, "005") == 0);

    sprintfn(buffer, sizeof(buffer), "%-03d", 5);
    assert(strcmp(buffer, "500") == 0);

    // signed, unsigned int

    sprintfn(buffer, sizeof(buffer), "%i", -123); // %i equiv to %d
    assert(strcmp(buffer, "-123") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", -123);
    assert(strcmp(buffer, "-123") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", 2147483647); // max signed int
    assert(strcmp(buffer, "2147483647") == 0);

    sprintfn(buffer, sizeof(buffer), "%d", 4294967295); // should not be supported
    assert(strcmp(buffer, "-1") == 0);

    sprintfn(buffer, sizeof(buffer), "%u", 2147483647);
    assert(strcmp(buffer, "2147483647") == 0);

    sprintfn(buffer, sizeof(buffer), "%u", 4294967295); // max unsigned int
    assert(strcmp(buffer, "4294967295") == 0);

    // hex, octal, binary

    sprintfn(buffer, sizeof(buffer), "%x", 0x12345678);
    assert(strcmp(buffer, "12345678") == 0);

    sprintfn(buffer, sizeof(buffer), "%o", 0777);
    assert(strcmp(buffer, "777") == 0);

    sprintfn(buffer, sizeof(buffer), "%b", 0x99);
    assert(strcmp(buffer, "10011001") == 0);

    // pointers

    void *p = (void *)0x12345678;
    sprintfn(buffer, sizeof(buffer), "%p", p);
    assert(strcmp(buffer, "12345678") == 0);

    p = (void *)0xFFFFFFFF;  // full 4GB address
    sprintfn(buffer, sizeof(buffer), "%p", p); 
    assert(strcmp(buffer, "FFFFFFFF") == 0);
}

