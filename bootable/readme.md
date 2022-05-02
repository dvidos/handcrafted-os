

# bootable

An attempt to build a bootable... something.

The itch to put together enough assembly and a little C
to have a simple process running in a emulator or computer.

Might even be able to make a bootable usb stick!

Following this guy: https://dev.to/frosnerd/writing-my-own-boot-loader-3mld

## description

Essentially, we write 512 bytes of assembly, in real mode,
that are loaded into address 0x7c00 from the BIOS.

Those 512 bytes are used to load a bunch of disk sectors into memory,
again written in assembly, that will allow us to load a larger kernel.

Finally, the kernel, written in C, will kick things off.


## history

Early April 2022 I wrote a basic bootloader in assembly.
It would enter protected mode and could load additional
sectors from disk. Upon trying to link a C generated
primitive kernel, I stumbled upon the reality world of
i386 vs i686 cross compilation issues.

Therefore I started reading http://wiki.osdev.org/


## things to implement (maybe in order)

* a way to launch and run tests suites (qemu passes arguments through the -append argument)
* std funcs, towards a library for user programs:
    * mem\*
    * str\*
    * atoi, itoa, atol, ltoa
    * malloc/free
    * printf, puts, putchar
    * panic, assert
* text mode screen driver
* keyboard input
* scheduler and ability to start / stop processes
* primitive file system (maybe in memory for now, maybe ext2 later))
    * fopen, fread, fwrite, fseek, fclose
* a build script to bring all things together, since a makefile will not cut it (e.g. [minix3 build.sh](https://github.com/Stichting-MINIX-Research-Foundation/minix/blob/master/build.sh))

* primitive memory management (maybe not even virtual for now)
* some kind of initialization sequence (init, initrd, etc)


## goals

* learn and implement how processes communicate with each other
* learn and implement how processes communicate with the kernel
* learn how multitasking is implemented, implement at least one scheduler

ultimate goal ? maybe ability to edit a file, compile and run. though that's a long goal. maybe [smaller C](https://wiki.osdev.org/Smaller_C) is an answer

## architecture notes

It seems minix is a microkernel. There are separate tasks for memory management, filesystem, clock, etc.
Each time a process wants to do something, it will make a call (say, to malloc(), open(), fork(), sleep() etc).
This leads to a message being sent to the target task. The sender is blocked and put to sleep.
If the task is already handling some other message, the message is put in a queue.
Each task has a loop for receiving messages and acting on them. I think that's the module that insmod
is doing for linux as well.
When the task has been performed, the response is sent back to the original sender, via IPC
and the sender is awakened.

It seems Tanembaum wanted the microkernel architecture because drivers are the main problem of
today's computers, in the sense that they have to be priviledged, but they are also plagued with bugs.
Therefore, minix3 has a mother task, that is responsible for spawning and respawning any drivers that fail.

## brief overview

It seems the kernel does (at least) the following things:

* initializes things and makes available various functionalities to programs
* maintains and runs the scheduler
* maintains and runs the interrupt service routines
* maintains a series of tasks / drivers, to abstract away things and offer uniform support, e.g.:
    * memory allocation / freeing
    * file system operations (in unix, even non-file operations happen through the file system)
    * terminal abstraction and console i/o
    * timers (sleep or wait for something)
* offers the messaging mechanism, through a soft interrupt, to offer system calls
    * file operations
    * memory, clock operations
    * processes operations (start, fork, kill, signal, etc)
* offers useful abstractions over the hardware, e.g.
    * multiple applications running at the same time
    * infinite screen size on terminal, through scrolling
    * many terminals, ALt+1 through Alt+6 etc
    * random access R/W on files, on a flat, fixed size disk
    * pseudo infinite memory through paging and caching out

Also the following must be met:

* stack set up before we launch the C function
* known code segment of kernel, I see people do 0x0010 or 0x0100 as segment.
* entering protected mode (boot loader actually)
* GDT (general description table, for paging and access etc)
* IDT (interrupt definition table, functions to handle interrupts)

## the list

### things to do

* bootloader, iso image & boot from usb stick
* multi tasking (exec, fork, etc)
* scheduler
* messages between tasks
* file system driver (read / write some filesystem)
* libc for usermode
* loading and executing programs
* user mode file system operations (open, read, write, close etc)
* primitive tools (mini shell, vi, mini c compiler etc)
* greek language keyboard mapping and utf8 support, even in console
* tests
* kernel console, with various capabilities:
    * show memory map (fragmentation etc)
    * dump processes tables and info
    * start/stop processes etc
    * read and write specific memory address or port (byte, word, long)
    * maybe send messages (IPC) directly to tasks
    * report uptime, run tests or benchmarks, etc
* the four main tasks that user mode code can use (also described in Minix Book)
    * process management (fork, exec, exit, etc)
    * I/O device management
    * memory management (malloc etc)
    * file management
* other topics
    * Real time clock - reading, writing (see https://wiki.osdev.org/CMOS)
    * Beeper control for fun (maybe from PIT channel 2)

### things done

* bootsector code (frozen for now, working with the kernel arg of qemu)
* global descriptor table
* interrupt descriptor table
* screen driver
* keyboard driver
* timers



## OS structure

Maybe, if we take the idea of pipes in unix and extend it to the idea of microservices of today,
where instead of stdout/stderr we have logging output with various info/error levels, 
where we have no only plain text data, but also json, yaml, xml (bliah), csv, and other formats,
maybe we could emulate a villabe of microservices, where each one can be one tool to do thing,
each one would have internal storage, transparent, like file access, but without needing to care 
about uid/guid and file and folder names etc, everything being isolated,
then maybe we could have filters and converters and enrichers and hydrators, and tees, and data sinks
and data sources and maybe something useful along the way (like responding to user input).

Essentially, maybe something between the Cloud Functions that Google advertises and maybe the pipes 
that the guys implemented back in the day. Some semantic way of piping together the flow of some information,
and maybe not for a single execution (like a shell command execution), but for being resident up there,
so that whenever a process spurted out some data, the connected services would receive and process
etc, etc until we got the desired result.

Just an idea.






