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
