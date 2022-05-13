

# bootable

An attempt to build a bootable... something.

The itch to put together enough assembly and a little C
to have a simple process running in a emulator or computer.

Might even be able to make a bootable usb stick!

~Following this guy: [https://dev.to/frosnerd/writing-my-own-boot-loader-3mld](https://dev.to/frosnerd/writing-my-own-boot-loader-3mld)~ outgrew this idea, now following mostly [OSDEv.org](https://osdev.org/)

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

* multi tasking
* kernel memory manager (kalloc, kfree) - then update mutlitask.c to use dynamic memory
* load and execute executables
* own repository
* document with brief paragraph describing kernel's concerns (e.g. memory, keyboard, screen, 
locking, task switching, idt, gdt, boot info, etc)
* ata disk driver (detect, implement R/W operations) (info [one](http://www.osdever.net/tutorials/view/lba-hdd-access-via-pio), [two](https://wiki.osdev.org/ATA_PIO_Mode))
* scheduler
* messages between tasks (IPC)
* file system driver (read / write some filesystem)
* libc for usermode
* loading and executing programs
* user mode file system operations (open, read, write, close etc)
* primitive tools (tiny shell, ls, cat, echo, tiny vi etc)
* greek language keyboard mapping and utf8 support, even in console
* the four main tasks that user mode code can use (also described in Minix Book)
    * process management (fork, exec, exit, etc)
    * I/O device management
    * memory management (malloc etc)
    * file management
* other topics
    * Beeper control for fun (maybe from PIT channel 2)
* kernel monitor
    * have a low level hotkey (e.g. Ctrl+Alt+Shift+M) to toggle some area of the screen to be used for kernel monitoring
    * maybe it can have pages, left and right or something like that
    * programs should not even be aware of it

## Dreamland wish list (push the envelope)

* Multiple entries in a task or app, similarly to how Android is doing. 
* Something similar to Google Cloud functions: implement a small thing, configure when it is to be called (e.g. when a message of x becomes available, or when we boot or etc etc) and have it run.
* Kafka pipelines emulation would be easily implementable with shell pipes.
* Better handling of individual chunks of information, e.g. json objects with headers. Instead of just reading one stream of bytes, read a stream of those objects.
* If we can have an editor and a compiler, edit, compile and run software on this new os, it will be amazing!

### things done

* reading real time clock + interrupt every second.
* tests
* cli/sti and spinlock methods
* started kernel console, nice to play with it!
    * show memory map (fragmentation etc)
    * dump processes tables and info
    * start/stop processes etc
    * read and write specific memory address or port (byte, word, long)
    * maybe send messages (IPC) directly to tasks
    * report uptime, run tests or benchmarks, etc
* bootloader, iso image & booted from usb stick
* bootsector code (frozen for now, working with the kernel arg of qemu)
* global descriptor table
* interrupt descriptor table
* screen driver
* keyboard driver
* timers (ticks, pause)



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


## sources of knowledge / reference

* [OSDev.org](https://wiki.osdev.org/Main_Page) for the most part
* Minix1 (mostly for an 8086) ([github repo](https://github.com/gdevic/minix1))
* Minix3 (for 386) ([github repo](https://github.com/Stichting-MINIX-Research-Foundation/minix))
* The Lions Book (unix v6) ([online pdf](http://worrydream.com/refs/Lions%20-%20A%20Commentary%20on%20the%20Sixth%20Edition%20UNIX%20Operating%20System.pdf), [online code](http://www.v6.cuzuco.com/v6.pdf))
* Linux 0.01 ([github repo](https://github.com/zavg/linux-0.01))
* Linux Kernel Map (though much modern and complex) [link](https://makelinux.github.io/kernel/map/)
* MIT's course OS xv6, for a RISK machine ([pdf](https://pdos.csail.mit.edu/6.828/2021/xv6/book-riscv-rev2.pdf), [code repo](https://github.com/mit-pdos/xv6-riscv)). An abandonned x86 version is [here](https://github.com/mit-pdos/xv6-public)
* [The Little book about OS development](https://littleosbook.github.io/)
* [Intel 80386 Programmer's Reference Manual](https://logix.cz/michal/doc/i386/), an unofficial copy with nicely explained, low level information and details.

## possible folder organization

project root
- docs
    - (various documents)
- boot_disk  (boot disk contents)
    - home
    - boot
    - bin
    - ...other folders
- src
    - kernel (kernel, builds kernel.bin)
    - user  (user land)
        - utils (e.g. mini vim)
        - libc  (for user utils)
- build.sh (build script)


# various unrelated notes

* before version 2.5, where it adopted special instructions to move control to kernel mode,
linux was using interrupt x80 for syscalls (open, close, read etc) where the number of syscall to execute
was placed on EAX, before executing INT. this from wikipedia: https://en.wikipedia.org/wiki/System_call
(that page has a nice list of categorized syscalls)
* if i want to access usb for storage, maybe I need to write a usb stack.
there is some implementation [here](https://github.com/thepowersgang/acess2/blob/master/KernelLand/Modules/USB/MSC/main.c), that I found from old OSDev page, it's an abandoned project.
* it seems a program has heap space allocated to it (similarly to how it has code, data and stack space allocated to it), and that this heap space can be changed in size dynamically. this was originally done through the brk/sbrk methods, later with the mmap method.
therefore it seems there is some usefulness to mamory paging after all... :-)
* [this](https://github.com/mit-pdos/xv6-public/blob/master/spinlock.c) file has some nice cli/sti, pushcli/popsti,
as well as spinlock code. Although the repo is weird, lots of little gems in there! :-) also 
[this one](https://github.com/tiqwab/xv6-x86_64), for 64 bits
