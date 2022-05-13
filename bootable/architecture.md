

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

### memory

### screen

### keyboard

### timer

### clock

### hdd

### filesys

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



