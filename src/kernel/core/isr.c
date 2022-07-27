#include <ctypes.h>
#include <idt.h>
#include <drivers/timer.h>
#include <drivers/kbd_drv.h>
#include <drivers/clock.h>
#include <memory/virtmem.h>
#include <pic.h>
#include <klog.h>
#include <bits.h>
#include <multitask/multitask.h>


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
            klog_error("Page fault");
            virtual_memory_page_fault_handler(regs.err_code);
            break;
        case 0x0D:
            // General Protection Fault, see https://wiki.osdev.org/Exceptions#General_Protection_Fault
            char *tables[] = { "GDT", "IDT", "LDT", "IDT" };
            int table = BIT_RANGE(regs.err_code, 2, 1);
            int entry = BIT_RANGE(regs.err_code, 15, 3);
            klog_error("Received General Protection Fault (int 0x%x), error_code=0x%x, is_external=%d, table=%s, index=%d", 
                regs.int_no, regs.err_code,
                regs.err_code & 0x1,
                table < 4 ? tables[table] : "?",
                entry
            );
            break;
        default:
            klog_warn("Received interrupt %d (0x%x), error %d", regs.int_no, regs.int_no, regs.err_code );
    }

    // we need to send end-of-interrupt acknowledgement 
    // to the PIC, to enable subsequent interrupts
    pic_send_eoi(regs.int_no);
}


