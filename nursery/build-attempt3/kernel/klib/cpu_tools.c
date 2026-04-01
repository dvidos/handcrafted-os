#include "../include/ctypes.h"
#include "../logger/logger.h"
#include "cpu_tools.h"


MODULE("CPU_TOOLS", LOG_LEVEL_TRACE);


void log_interrupt_status(const char* location) {
    uint32_t eflags;

    asm volatile(    
        "pushfl\n\t"
        "popl %0"
        : "=g"(eflags)
    );

    int interrupts_enabled = (eflags & 0x200);  // Bit 9 is the IF (Interrupt Flag)
    log_info("[%s] EFLAGS: 0x%08x | Interrupts: %s", location, eflags, interrupts_enabled ? "ENABLED" : "DISABLED");
}
