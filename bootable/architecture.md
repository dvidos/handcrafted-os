

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

(in general free, parse boot info, reserve 1-2MB for kernel, paging, virtual & physical, CR3, PD, PT)

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
