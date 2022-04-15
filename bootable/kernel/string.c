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


