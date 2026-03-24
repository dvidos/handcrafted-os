#ifndef _TRAP_FRAME_H
#define _TRAP_FRAME_H

#include "../include/ctypes.h"
#include "../logger/logger.h"


// things pushed in the isr_stub we have in assembly
// this is passed when interrupt_handler_c is called from our assembly stub
// same for all interrupts, including the syscall one (0x80)
// when assembly pushes ESP and calls C handler, the handler receives
// the value of ESP, i.e. a pointer to the current stack position.
// what things have been pushed is decoded by the struct below.

typedef struct trap_frame {

   // pushed by our isr stub, before calling C func
   uint32_t gs;
   uint32_t fs;
   uint32_t es;
   uint32_t ds;

   uint32_t edi;
   uint32_t esi;
   uint32_t ebp;
   uint32_t esp_dummy; // this is because of pusha, ignore
   uint32_t ebx;
   uint32_t edx;
   uint32_t ecx;
   uint32_t eax; // Pushed by pushad.
   
   // pushed by our assembly macros
   uint32_t int_no;
   uint32_t err_code;  // Interrupt number and error code (if applicable)
   
   // pushed by CPU before jumping to interrupt entry.
   // eip, cs, flags pushed always. esp+ss crossing ring3 -> ring0
   uint32_t eip;
   uint32_t cs;
   uint32_t eflags;
   uint32_t user_esp; // points inside user_stack, not a trapframe, when interrupt occured. only meaningful in user processes
   uint32_t ss;

} __attribute__((packed)) trap_frame_t;


void trap_frame_log_formatter(log_write_stream_t *stream, va_list args);




#endif
