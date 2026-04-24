#include "stack_frames.h"
#include "../arch/gdt.h"
#include "../logger/logger.h"

static const char *segment_name(uint32_t seg) {
    // remove RPL 3, if present (2 first bits)
    seg &= ~0x3;

    if (seg == KERNEL_CODE_SEGMENT || seg == KERNEL_DATA_SEGMENT)
        return "(kernel)";
    else if (seg == USER_CODE_SEGMENT || seg == USER_DATA_SEGMENT)
        return "(user)";
    else 
        return "(unknown)";
}

void interrupt_frame_log_formatter(log_write_stream_t *stream, va_list args) {
       
    interrupt_frame_t *frame = va_arg(args, interrupt_frame_t *);

    stream->printf(stream, "EAX 0x%08x   ESI 0x%08x   GS 0x%02x %-10s", frame->eax, frame->esi,       frame->gs, segment_name(frame->gs));
    stream->printf(stream, "ECX 0x%08x   EDI 0x%08x   FS 0x%02x %-10s", frame->ecx, frame->edi,       frame->fs, segment_name(frame->fs));
    stream->printf(stream, "EDX 0x%08x   EBP 0x%08x   ES 0x%02x %-10s", frame->edx, frame->ebp,       frame->es, segment_name(frame->es));
    stream->printf(stream, "EBX 0x%08x   ESP (ignored)    DS 0x%02x %-10s", frame->ebx, frame->ds, segment_name(frame->ds));
    stream->printf(stream, "interrupt: %d (0x%x), error code: %d (0x%x)", frame->int_no, frame->int_no, frame->err_code, frame->err_code);
    stream->printf(stream, "(the values below are meaningful for user processes only)");
    stream->printf(stream, "SS 0x%02x %-10s   ESP 0x%08x  eflags 0x%08x", frame->ss, segment_name(frame->ss), frame->user_esp, frame->eflags);
    stream->printf(stream, "CS 0x%02x %-10s   EIP 0x%08x", frame->cs, segment_name(frame->cs), frame->eip);
}

void c_frame_log_formatter(log_write_stream_t *stream, va_list args) {
       
    c_frame_t *frame = va_arg(args, c_frame_t *);

    stream->printf(stream, "EBX 0x%08x   ESI 0x%08x", frame->ebp, frame->esi);
    stream->printf(stream, "EBP 0x%08x   EDI 0x%08x", frame->ebx, frame->edi);
    stream->printf(stream, "return address:  EIP 0x%08x", frame->eip);
}


