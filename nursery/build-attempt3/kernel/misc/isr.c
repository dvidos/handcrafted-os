#include "../include/ctypes.h"
#include "idt.h"
#include "../drivers/timer.h"
#include "../drivers/kbd_drv.h"
#include "../drivers/clock.h"
#include "../memory/virtmem.h"
#include "pic.h"
#include "../utils/logger.h"
#include "../include/bits.h"
#include "../multitask/multitask.h"


MODULE("ISR", LOG_LEVEL_WARN);


static void log_registers(registers_t regs) {
    log_error("Registers follow:\n" // we need the newline to align the registers left of log preambles
        "  EAX = 0x%08x    ESI = 0x%08x    EIP = 0x%08x\n"
        "  EBX = 0x%08x    EDI = 0x%08x    CS  = 0x%08x\n"
        "  ECX = 0x%08x    ESP = 0x%08x    DS  = 0x%08x\n"
        "  EDX = 0x%08x    EBP = 0x%08x    SS  = 0x%08x\n",
        regs.eax,  regs.esi,  regs.eip,
        regs.ebx,  regs.edi,  regs.cs,
        regs.ecx,  regs.esp,  regs.ds,
        regs.edx,  regs.ebp,  regs.ss
    );
}


void isr_handler(registers_t regs) {
    // don't forget we have mapped IRQs 0+ to 0x20+
    // to avoid the first 0x1F interrupts that are CPU faults in protected mode
    switch (regs.int_no) {
        case 0x20:
            timer_interrupt_handler(&regs);
            multitasking_timer_ticked();
            break;
        case 0x21:
            keyboard_handler(&regs);
            break;
        case 0x28:
            real_time_clock_interrupt_interrupt_handler(&regs);
            break;
        case 0x0E:
            // Page Fault: https://wiki.osdev.org/Exceptions#Page_Fault
            log_warn("Page fault detected");
            virtual_memory_page_fault_handler(regs.err_code);
            break;
        case 0x0D:
            // General Protection Fault, see https://wiki.osdev.org/Exceptions#General_Protection_Fault
            char *tables[] = { "GDT", "IDT", "LDT", "IDT" };
            bool external = regs.err_code & 0x1;
            int table = BIT_RANGE(regs.err_code, 2, 1);
            int entry = BIT_RANGE(regs.err_code, 15, 3);
            log_error("Received General Protection Fault (int 0x%x), error_code=0x%x, is_external=%d, table=%s, index=%d", 
                regs.int_no, 
                regs.err_code,
                external,
                table < 4 ? tables[table] : "?",
                entry
            );
            log_registers(regs);
            // based on EIP (e.g. it was at 0x115A34, therefore kernel text segment)
            // to find out which function had the offending instruction, i did
            // $ `nm -n src/kernel/kernel.bin`
            // to dissassemble the code and find the exact offset, i did
            // $ `objdump -D src/kernel/core/idt_low.o`
            // by calculating offsets
            break;
        case 0x6:
            log_error("Received interrupt %d: Invalid Opcode Exception: The CPU tried to execute an instruction that is not valid", regs.int_no);
            log_registers(regs);
            // we could deduce from where this happens, kernel or process
            // then one could dissassemble the executable and look around the faulting address
            // i686-elf-objdump -d init | less
            break;
        default:
            log_error("Received interrupt %d (0x%x), error %d", regs.int_no, regs.int_no, regs.err_code);
            log_registers(regs);
    }

    // we need to send end-of-interrupt acknowledgement 
    // to the PIC, to enable subsequent interrupts
    pic_send_eoi(regs.int_no);
}


