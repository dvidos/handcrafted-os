#include "../libc_internal.h"

/**
 * @brief Gives execution to other programs
 */
void yield() {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}
