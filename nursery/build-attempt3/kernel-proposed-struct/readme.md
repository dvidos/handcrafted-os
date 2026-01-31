# kernel

Because, what is more challenging?


## code organization

Few rules to keep the sanity of the kernel.

* Use directories liberally, to group functionality.
* Each directory will have one public file named after itself (e.g. mem/mem.h)
* Other includes are private and should not be used outside the directory.
* Use as much `static` as possible
* No public variables, no public functions without strong prefixes.
* Use vtables and structs with pointers, to namespace functions.

## features to support

* Boot and Core CPU Setup
  * Bootloader → enter protected mode
  * GDT + TSS setup
  * IDT with basic exception handlers
  * Interrupt handling (PIC/APIC)
  * Timer interrupt source (PIT/HPET)
  * Basic ISR framework (register save/restore)

* Memory Management
  * Physical memory map detection
  * Physical page allocator
  * Paging enabled (kernel virtual memory)
  * Kernel heap allocator (kmalloc/kfree)
  * Per-process page directory
  * User vs kernel address space split

* Process and Execution Model
  * Process/task struct + kernel stack per process
  * Context switch mechanism
  * Simple scheduler (round-robin OK)
  * User mode entry (ring 3 via iret)
  * ELF32 program loader
  * Syscall entry (int 0x80)
  * Minimal syscall dispatcher table

* Filesystem and I/O Model
  * VFS-style file abstraction layer
  * One real filesystem driver (FAT32 or ext2)
  * Path resolution and directories
  * Per-process file descriptor table
  * Core file syscalls: open/read/write/close/lseek
  * Device-as-file interface (/dev/console, /dev/null)

* POSIX-like Process + IPC Features
  * exit + exit codes
  * fork or spawn
  * exec (replace process image)
  * waitpid
  * getpid
  * Pipes (pipe() with blocking semantics)
  * Basic signals: SIGKILL, SIGTERM, SIGCHLD
  * TTY line input support (for shell use)


