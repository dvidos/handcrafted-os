#include "../libc_internal.h"
#include <stdlib.h> // For strtoll

long long atoll(const char *str) {
    return strtoll(str, NULL, 10);
}