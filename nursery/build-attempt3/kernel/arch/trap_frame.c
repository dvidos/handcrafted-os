#include "trap_frame.h"
#include "../arch/gdt.h"
#include "../logger/logger.h"

static const char *segment_name(uint32_t seg) {
    if (seg == KERNEL_CODE_SEGMENT || seg == KERNEL_DATA_SEGMENT)
        return "(kernel)";
    else if (seg == USER_CODE_SEGMENT || seg == USER_DATA_SEGMENT)
        return "(user)";
    else 
        return "(unknown)";
}

void trap_frame_log_formatter(log_write_stream_t *stream, va_list args) {
       
    trap_frame_t *tf = va_arg(args, trap_frame_t *);

    stream->printf(stream, "EAX 0x%08x   ESI 0x%08x   GS 0x%02x %-10s", tf->eax, tf->esi,       tf->gs, segment_name(tf->gs));
    stream->printf(stream, "ECX 0x%08x   EDI 0x%08x   FS 0x%02x %-10s", tf->ecx, tf->edi,       tf->fs, segment_name(tf->fs));
    stream->printf(stream, "EDX 0x%08x   EBP 0x%08x   ES 0x%02x %-10s", tf->edx, tf->ebp,       tf->es, segment_name(tf->es));
    stream->printf(stream, "EBX 0x%08x   ESP 0x%08x   DS 0x%02x %-10s", tf->ebx, tf->esp_dummy, tf->ds, segment_name(tf->ds));
    stream->printf(stream, "interrupt: %d (0x%x), error code: %d (0x%x)", tf->int_no, tf->int_no, tf->err_code, tf->err_code);
    stream->printf(stream, "(the values below are meaningful for user processes only)");
    stream->printf(stream, "SS 0x%02x %-10s   ESP 0x%08x  eflags 0x%08x", tf->ss, segment_name(tf->ss), tf->user_esp, tf->eflags);
    stream->printf(stream, "CS 0x%02x %-10s   EIP 0x%08x", tf->cs, segment_name(tf->cs), tf->eip);
}


