#include <stdbool.h>




inline void disable_interrupts() {
    asm volatile ("cli");
}

inline void enable_interrupts() {
    asm volatile ("sti");
}
