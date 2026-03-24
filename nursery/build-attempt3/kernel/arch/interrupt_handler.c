#include "../include/ctypes.h"
#include "idt.h"
#include "../drivers/timer.h"
#include "../drivers/kbd_drv.h"
#include "../drivers/clock.h"
#include "../memory/virtmem.h"
#include "pic.h"
#include "../logger/logger.h"
#include "../utils/panic.h"
#include "../include/bits.h"
#include "../proc/multitask.h"
#include "../proc/procman/scheduler.h"


MODULE("ISR", LOG_LEVEL_DEBUG);


int erroneous_interrupts = 0;

void interrupt_handler_c(trap_frame_t *tf) {

    // don't forget we have mapped IRQs 0+ to 0x20+
    // to avoid the first 0x1F interrupts that are CPU faults in protected mode
    switch (tf->int_no) {
        case 0x20:
            timer_interrupt_handler(tf);
            multitasking_timer_ticked();
            break;
        case 0x21:
            keyboard_handler(tf);
            break;
        case 0x28:
            real_time_clock_interrupt_interrupt_handler(tf);
            break;
        case 0x0E:
            // Page Fault: https://wiki.osdev.org/Exceptions#Page_Fault
            vmm_page_fault_handler(tf);
            break;
        case 0x0D:
            // General Protection Fault, see https://wiki.osdev.org/Exceptions#General_Protection_Fault
            char *tables[] = { "GDT", "IDT", "LDT", "IDT" };
            bool external = tf->err_code & 0x1;
            int table = BIT_RANGE(tf->err_code, 2, 1);
            int entry = BIT_RANGE(tf->err_code, 15, 3);
            log_error("Received General Protection Fault (int 0x%x), error_code=0x%x, is_external=%d, table=%s, index=%d", 
                tf->int_no, 
                tf->err_code,
                external,
                table < 4 ? tables[table] : "?",
                entry
            );
            log_error("Trap frame follows:");
            log_error_fmt(trap_frame_log_formatter, "  ", tf);
            // based on EIP (e.g. it was at 0x115A34, therefore kernel text segment)
            // to find out which function had the offending instruction, i did
            // $ `nm -n src/kernel/kernel.bin`
            // to dissassemble the code and find the exact offset, i did
            // $ `objdump -D src/kernel/core/idt_low.o`
            // by calculating offsets
            if (++erroneous_interrupts >= 3)
                panic("Too many erroneous interrupts");
            break;
        case 0x6:
            log_error("Received interrupt %d: Invalid Opcode Exception: The CPU tried to execute an instruction that is not valid", tf->int_no);
            log_error("Trap frame follows:");
            log_error_fmt(trap_frame_log_formatter, "  ", tf);
            // we could deduce from where this happens, kernel or process
            // then one could dissassemble the executable and look around the faulting address
            // i686-elf-objdump -d init | less
            if (++erroneous_interrupts >= 3)
                panic("Too many erroneous interrupts");
            break;
        case 0x80:
            extern void isr_syscall(trap_frame_t *tf);
            isr_syscall(tf);
            break;
        default:
            log_error("Received interrupt %d (0x%x), error %d", tf->int_no, tf->int_no, tf->err_code);
            log_error("Trap frame follows:");
            log_error_fmt(trap_frame_log_formatter, "  ", tf);
            if (++erroneous_interrupts >= 3)
                panic("Too many erroneous interrupts");
    }

    // we need to send end-of-interrupt acknowledgement 
    // to the PIC, to enable subsequent interrupts
    pic_send_eoi(tf->int_no);
}

