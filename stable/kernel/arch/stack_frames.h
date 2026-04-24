#ifndef _TRAP_FRAME_H
#define _TRAP_FRAME_H

#include "../include/ctypes.h"
#include "../logger/logger.h"

/*
   The Stack Layout (Low to High Address)

   For new process
   - c_frame_t: (Lowest Address) edi, esi, ebx, ebp, and the eip pointing to fork_ret.
   - Return Address: The address of your assembly exit logic (the "stub").
   - interrupt_frame_t: (Highest Address) The registers, cs, eip, eflags, esp, and ss for Ring 3.

   For blocked processes (especially through kernel)
   - c_frame_t: (Lowest Address) edi, esi, ebx, ebp, and the eip pointing to fork_ret.
   - Various C frames e.g. sys_call(), and vfs_read(), and dev_tty_read()
   - interrupt_frame_t: (Highest Address) The registers, cs, eip, eflags, esp, and ss for Ring 3.

   This sandwich allows for blocking & restoring mid C function in kernel (e.g. block on read)
*/



typedef struct interrupt_frame {

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

} __attribute__((packed)) interrupt_frame_t;



typedef struct c_frame {
   // pushed/popped by our c_switcher
   uint32_t edi;
   uint32_t esi;
   uint32_t ebx;
   uint32_t ebp;

   // pushed before calling switch, popped by 'ret'
   uint32_t eip;
} __attribute__((packed)) c_frame_t; 




void interrupt_frame_log_formatter(log_write_stream_t *stream, va_list args);
void c_frame_log_formatter(log_write_stream_t *stream, va_list args);




#endif
