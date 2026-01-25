#include <limits.h>
#include <va_list.h>
#include <stdio.h>
#include <string.h>



int printf(char* format, ...) {
	char buffer[128];

    va_list args;
    va_start(args, format);
    vsprintfn(buffer, sizeof(buffer), format, args);
    va_end(args);

    return puts(buffer);
}

