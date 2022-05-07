#include <stdint.h>
#include <stdbool.h>
#include "screen.h"
#include "cpu.h"

#define INTERRUPT_ENABLE_FLAG 0x00000200 // Interrupt Enable


// stop interrupts in current CPU only
inline void cli(void) {
    __asm__ volatile("cli");
};

inline void sti(void) {
    __asm__ volatile("sti");
};

// read eflags to see if interrrupts are disabled
static inline bool interrupts_enabled(void) {

    // pushfq is for 64-bit register and x86_64 mode
    // pushfd is for 32-bit register and 32 bit mode
    // pushf  pushes 16 bits onto the stack

    uint32_t eflags;
    __asm__ volatile("pushf; pop %0" : "=r"(eflags));

    return eflags & INTERRUPT_ENABLE_FLAG;
}


static int cli_depth = 0;
static int zero_depth_enabled = 0;


// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void pushcli(void) {
    if (cli_depth == 0)
        zero_depth_enabled = interrupts_enabled();

    // we must cli() unconditionally, if we checked 
    // and then cleared, then we'd have a race condition!
    cli(); 
    cli_depth++;
}


void popcli(void) {
    if (cli_depth <= 0)
        panic("popcli() without pushcli()");

    if (interrupts_enabled())
        panic("Interrupts enabled when popcli() called");

    cli_depth--;
    if (cli_depth == 0 && zero_depth_enabled)
        sti();
}

