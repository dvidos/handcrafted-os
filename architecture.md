

# Architecture

This file to describe the inner workings of this project. The main parts are:

* [boot](#boot) - code to start the kernel
* [kernel](#kernel) - operating system
* [libc](#libc) - library for user programs
* [user](#user) - programs to be executed when operating system is booted

The main components of each part are the following:

## Boot<a id="boot"></a>

Currently solving this with GRUB2, and Multiboot specification.

Attempted and may come back to the typical MBR 512 bytes first sector of a disk

## Kernel<a id="kernel"></a>

A monolithic piece of software, that is overseeing the hardware and offers
a more friendly environment for programs to run on. Consists of various 
modules:

* startup - a piece of assembly that allows the bootload to jump to
* kernel - the system initialization code
* GDT - the general descriptor table, describing memory segments
* IDT - the interrupt descriptor table, describing interrupt handlers
* memory - memory management, paging, allocation
* screen - a simple console driver using the vga memory mapped address
* keyboard - driver for reading, decoding and enqueuing key presses
* timer - the programmable timer, ticking every millisecond
* clock - the real time clock, ticking every second
* ports - code for reading and writing data from / to CPU ports
* string - utility functions like strlen(), strcpy() etc
* process - multi tasking
* konsole - a psudo shell for interactive testing of things
* tests - a file with various unit tests

A rundown of concerns of each subsystem would be in order.

### startup

In general, when kernel is starting up, it is responsible
of understanding the environment it runs in (e.g. what memory is available), 
initialize the various subsystems (e.g. timers) and start multitasking.

The information available are:

* interrupts are disabled,
* boot info contains a lot of information (memory, boot drive, parameters, etc)
* we can detect the kernel addresses fom two variables in the linker script

### PIC - Programmable Interrupt Controller

In real mode, we'd care about IRQs 0 through 15. 
Unfortunately, in protected mode, these are reserved by Intel
for errors, so we reprogram the pic, to move the IRQs
to the 16 - 31 range (0x20+).

It is important to enable the specific IRQs we care,
otherwise they will not fire. IRQ 2 is also the cascade
IRQ for the slave chip, so it should be enabled for IRQs 
8 and up, to fire.

### GDT - General Descriptor Table

(8 for kernel code, 16 for kernel data, needed for protected mode and IDT)

### IDT - Interrupt Descriptor Table

(needed to intercept keyboard and others)

### memory

Memory broadly consists of three things:

* physical memory manager, keeping track of allocations of pages, usually of 4KB each. 
this is done to make sure we don't read or write where we are not supposed to (e.g. bios data area)
* virtual memory mapping, to allow process to have virtual addresses and possibly swap to disk. 
this case needs an interrupt handler to handle page faults. 
* heap allocation (the malloc() calls), where a continuous chunk of memory is allocated 
at arbitrary sizes to one or more processes that request it.

The physical memory manager is using 4KB pages. 4GB would be broken down to 1024 pages.
If we use a 32-bit integer to track the status of 32 pages, we need 32768 of those 
integers, taking 128 KB of memory. But, searching for free pages is really fast,
since we are doing mainly bitwise operations.

In order to initialize the physical memory manager, we pass in the boot information 
and the kernel's starting and ending address. 
This leads to a map of what can be allocated and what not. Finally, 
kernel heap is requesting 1 MB of consecutive physical memory for the kernel's needs.

In order to support virtual addressing and mapping, there is lots of literature
around the Page Directory, the Page Tables and the CR3 register. 
We don't support virtual addressing or hot swapping of pages for now.


### screen

We use a simple driver writing to the VGA memory in address 0xB8000.

The screen has 25 lines and 80 columns and each character is stored
as two bytes, one for the color (background and foreground color nibbles)
and one for the actual character.

New lines, scrolling, backspace and other niceties are implemented 
in our software.

### keyboard

The keyboard fires IRQ 1. 

We need to read a port to receive the scan code. If the 
scan code is 0x0E, it's a shifting mechanism, and we'll
receive the scancode in the subsequent interrupt call.

In general, bit 7 indicates whether the key was pressed or released.

It is up to the keyboard to implement any queue to save keystrokes,
so that applications can ask for them.

### timer

The Programmable Timer is setup to fire about once every millisecond.
It fires IRQ 0

We use it to maintain a `uptime_milliseconds` variable, to enable
delays or measurements in software at millisecond precision.

We also use it to fire the scheduler, to either switch tasks 
when their timeslice expires, or to wake up tasks sleeping on the timer.

### clock

Real time clock can give us the date and time. 

It is also setup to fire an interrupt every second.
The actual clock fires at 2 Hz, but we keep track 
and divide the calls in software.

It fires IRQ 8 and everytime it fires, one must read the register C, 
for the interrupt to fire again.

### multi task

Task switching works by switching one stack frame for another (changing the stack pointer).

This leads to very useful, interesting, peculiar and completely counter-intuitive behavior.

For example, the scheduler is locked, the task is switched and then the scheduler is unlocked,
but by whatever task was switched in. The arguments and local variables after the switch
are different than before it. 

In general, our scheduler must be either all cooperative, or all preemptive. Since each task
cleans up whatever the previous task did, all tasks must be uniform. That means that,
if we want to have preemptive multitasking, all task switches must be done during the timer
IRQ, not by calling `schedule()` on the main thread.

Given that timesharing is implemented by the IRQ timer handler, all is left to most 
operations is manipulating the process lists (blocking, waking, terminating etc)

In the code, there is a `running_process` pointer, that always holds the running process.
We maintain a series of `ready_list`, for different priorities. Highest priority is zero. We maintain the `blocked_list`, for all blocked tasks (there will be a lot, but appending to the end of the list maintains some degree of FIFO fairness). Finally, there is the `terminated_list`, for tasks that have 
terminated (and will be cleaned up by the idle process).

A piece of code that wants to trigger a task switch, must frame the `schedule()` call with
`lock_scheduler()` and `unlock_scheduler`. That is, it's the responsibility of 
the caller to lock the scheduler. 
Don't forget that it's a different task that unlocks the scheduler.

There are a few methods a task can use to update its status:

  * `yield()` voluntarily giving up the CPU
  * `sleep()` blocks the task for a number of milliseconds
  * `exit()` terminates the task with an exit code
  * `block()` blocks the task for a specific reason and an optional channel

For extra functionality, there are the following methods:

  * `create_process()` allocates and initializes a new process and returns it
  * `start_process()` adds the process to the ready list, to be switched in soon
  * `running_process()` returns the running_process
  * `lock_scheduler()` and `unlock_scheduler()` for whenever we will effect changes to the running status of the tasks
  * `unblock_process()` unblocks a task and puts it on the `ready_list`
  * `acquire_semaphore()` and  `release_semaphore()`. It allows the task to claim the semaphore if applicable, or blocks until the semaphore is available. Releasing the semaphore possibly unblocks  processes that are blocked waiting for it.
  * `acquire_mutex()` and `release_mutex()` are identical to semaphores, with a limit of one.


### hdd

(ATA PIO is slow, DMA for later)

### filesys

(FAT for starting, maybe ext2 for later on)

## LibC<a id="libc"></a>

We shall not have the standard C library in our operating system.
Therefore we need to build its replacement. 

Some methods, such as `strlen()` are very simple to implement.

Other methods, such as `open()` need to trap into the operating system.
Most probably, they will call an interrupt that the kernel will 
intercept, perform the action and then return the result.

Here is a list of methods we can / should support:

* strlen(), strcmp(), strcpy(), strncpy(), etc
* isalpha(), islower(), etc
* memcpy(), memcmp(), memmove(), etc
* itoa(), ltoa(), atol(), atoi(), etc
* printf(), vprintf(), sprintf() etc
* fopen(), fseek(), fread(), fwrite(), fclose()
* getchar(), putchar(), gets(), puts()
* malloc(), free(), calloc(), realloc() etc
* spawn(), sleep(), exit(), kill(), getpid(), etc
* getenv(), putenv(), etc
* uptime(), time(), etc
* rand(), seed() etc

Ideally, we'd like to be able to work with utf-8 as well

A list of methods in the C library is [here](https://www.gnu.org/software/libc/manual/html_node/Function-Index.html)

# User<a id="user"></a>

A single operating system by itself is not that useful. Ideally,
it will contain a few things that will both be useful to the owner / user, 
and also provide pleasure in writing and running them.

A list of tools I think we'll need is following:

* shell (any simple one)
* vi (any simple clone)
* ls, echo, cat, tee, 
* if we manage to have a simple C compiler and assembler, it'd be stellar
* if we manage to have tcp/ip and put some basic email functionality, fun!
