#include <klib/string.h>
#include <klog.h>


// for documentation, see https://wiki.osdev.org/IDT

// things pushed in the isr_stub we have in assembly
// this is passed when isr_handler is called from our assembly stub

MODULE("IDT");


#define GATE_TYPE_TASK              0x5  // offset value should be zero in this case
#define GATE_TYPE_16BIT_INTERRUPT   0x6
#define GATE_TYPE_16BIT_TRAP        0x7
#define GATE_TYPE_32BIT_INTERRUPT   0xE
#define GATE_TYPE_32BIT_TRAP        0xF

// +----------+----------+----------------------------+----------+----------+----------+----------+----------+
// | byte 7   |  byte 6  |           byte 5           | byte 4   | byte 3   | byte 2   | byte 1   | byte 0   |
// +----------+----------+----+-------+----+----------+----------+----------+----------+----------+----------+
// | 63 .. 56 | 55 .. 48 | 47 | 46 45 | 44 | 43 .. 40 | 39 .. 32 | 31 .. 24 | 23 .. 16 | 15 .. 8  | 7 .. 0   |
// | offset   | offset   | P  | DPL   | 0  | type     | reserved | selector | selector | offset   | offset   |
// | 4th byte | 3rd byte |    |       |    |          |          | 2nd byte | low byte | 2nd byte | low byte |
// +----------+----------+----+-------+----+----------+----------+----------+----------+----------+----------+
// also https://wiki.osdev.org/IDT
struct idt_gate_descriptor32 {
    uint16_t offset_lo;
    uint16_t segment_selector;
    uint8_t  reserved;
    uint8_t  gate_type: 4;
    uint8_t  zero: 1;
    uint8_t  privilege_level: 2;
    uint8_t  present: 1;
    uint16_t offset_hi;
} __attribute__((packed));

struct idt_gate_descriptor64 {
    uint16_t offset_lo;
    uint16_t segment_selector;
    uint8_t  ist;
    uint8_t  gate_type: 4;
    uint8_t  zero: 1;
    uint8_t  privilege_level: 2;
    uint8_t  present: 1;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t reserved;
} __attribute__((packed));

struct idt_descriptor32 {
    uint16_t size;
    uint32_t offset;
} __attribute__((packed));

struct idt_descriptor64 {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed));

struct idt_descriptor32 idt_descriptor;
struct idt_gate_descriptor32 gates[256];

// these defined in assembly
// defined my macros, they call the isr_common_stub(), again in assembly
// it collects registers and calls isr_handler(), which is in C
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void irq32();
extern void irq33();
extern void irq34();
extern void irq35();
extern void irq36();
extern void irq37();
extern void irq38();
extern void irq39();
extern void irq40();
extern void irq41();
extern void irq42();
extern void irq43();
extern void irq44();
extern void irq45();
extern void irq46();
extern void irq47();
extern void isr0x80();

// defined in assembly
extern void load_idt_descriptor(uint32_t);


void set_gate(uint8_t entry, uint32_t offset, uint16_t selector, uint8_t gate_type, uint8_t priv_level) {
    gates[entry].offset_lo = (offset & 0xFFFF);
    gates[entry].segment_selector = selector;
    gates[entry].reserved = 0;
    gates[entry].gate_type = gate_type;
    gates[entry].zero = 0;
    gates[entry].privilege_level = priv_level;
    gates[entry].present = 1;
    gates[entry].offset_hi = ((offset >> 16) & 0xFFFF);
}


// prepares and loads the Interrupt Descriptor Table
void init_idt(uint16_t code_segment_selector) {

    memset((char *)gates, 0, sizeof(gates));

    set_gate( 0, (uint32_t)isr0,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 1, (uint32_t)isr1,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 2, (uint32_t)isr2,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 3, (uint32_t)isr3,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 4, (uint32_t)isr4,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 5, (uint32_t)isr5,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 6, (uint32_t)isr6,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 7, (uint32_t)isr7,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 8, (uint32_t)isr8,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate( 9, (uint32_t)isr9,  code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(10, (uint32_t)isr10, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(11, (uint32_t)isr11, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(12, (uint32_t)isr12, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(13, (uint32_t)isr13, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(14, (uint32_t)isr14, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(15, (uint32_t)isr15, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(16, (uint32_t)isr16, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(17, (uint32_t)isr17, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(18, (uint32_t)isr18, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(19, (uint32_t)isr19, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(20, (uint32_t)isr20, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(21, (uint32_t)isr21, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(22, (uint32_t)isr22, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(23, (uint32_t)isr23, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(24, (uint32_t)isr24, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(25, (uint32_t)isr25, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(26, (uint32_t)isr26, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(27, (uint32_t)isr27, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(28, (uint32_t)isr28, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(29, (uint32_t)isr29, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(30, (uint32_t)isr30, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(31, (uint32_t)isr31, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);

    set_gate(32, (uint32_t)irq32, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(33, (uint32_t)irq33, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(34, (uint32_t)irq34, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(35, (uint32_t)irq35, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(36, (uint32_t)irq36, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(37, (uint32_t)irq37, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(38, (uint32_t)irq38, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(39, (uint32_t)irq39, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(40, (uint32_t)irq40, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(41, (uint32_t)irq41, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(42, (uint32_t)irq42, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(43, (uint32_t)irq43, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(44, (uint32_t)irq44, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(45, (uint32_t)irq45, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(46, (uint32_t)irq46, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);
    set_gate(47, (uint32_t)irq47, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);

    set_gate(0x80, (uint32_t)isr0x80, code_segment_selector, GATE_TYPE_32BIT_INTERRUPT, 0);

    idt_descriptor.size = sizeof(gates) - 1;
    idt_descriptor.offset = (uint32_t)gates;
    load_idt_descriptor((uint32_t)&idt_descriptor);
}

void dump_registers(registers_t *regs) {
    klog_debug("DS      : %08x", regs->ds);
    klog_debug("CS      : %08x", regs->cs);
    klog_debug("EDI     : %08x", regs->edi);
    klog_debug("ESI     : %08x", regs->esi);
    klog_debug("EBP     : %08x", regs->ebp);
    klog_debug("ESP     : %08x", regs->esp);
    klog_debug("EBX     : %08x", regs->ebx);
    klog_debug("EDX     : %08x", regs->edx);
    klog_debug("ECX     : %08x", regs->ecx);
    klog_debug("EAX     : %08x", regs->eax);
    klog_debug("INT_NO  : 0x%x", regs->int_no);
    klog_debug("ERR_CODE: 0x%x", regs->err_code);
    klog_debug("EIP     : %08x", regs->eip);
    klog_debug("EFLAGS  : %08x", regs->eflags);
    klog_debug("USERRESP: %08x", regs->useresp);
    klog_debug("SS      : %08x", regs->ss);
}
