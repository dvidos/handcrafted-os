#include <stdint.h>
#include <stdbool.h>
#include "screen.h"
#include "ports.h"
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
inline bool interrupts_enabled(void) {

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



void disable_nmi() {
    // Since the 80286 the mechanism used was through IO ports associated with the CMOS/Realtime Clock(RTC) controller. 
    // This same mechanism is still mimicked in hardware today.
    // The top 2 bits of the CMOS/RTC address don't form part of the actual address. 
    // The top most bit was re-purposed to be the NMI toggle. 
    // If you enable bit 7, NMI is disabled. If you clear bit 7 then NMIs are enabled.
    // unfortunately, port 0x71 is write only, if will always return 0xFF.
    // we cannot read from it to see if NMI is masked or not

    outb(0x70, inb(0x70) | 0x80);

    // the controller always expects a read or write on port 0x71, 
    // after every write to port 0x70, or it may go to an undefined state.
    inb(0x71);
}

void enable_nmi() {
    // Since the 80286 the mechanism used was through IO ports associated with the CMOS/Realtime Clock(RTC) controller. 
    // This same mechanism is still mimicked in hardware today.
    // The top 2 bits of the CMOS/RTC address don't form part of the actual address. 
    // The top most bit was re-purposed to be the NMI toggle. 
    // If you enable bit 7, NMI is disabled. If you clear bit 7 then NMIs are enabled.
    // unfortunately, port 0x71 is write only, if will always return 0xFF.
    // we cannot read from it to see if NMI is masked or not

    outb(0x70, inb(0x70) & 0x7F);

    // the controller always expects a read or write on port 0x71, 
    // after every write to port 0x70, or it may go to an undefined state.
    inb(0x71);

}
