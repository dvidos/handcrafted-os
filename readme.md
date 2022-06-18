

# handcrafted opreating system

An attempt to build a handcrafted operating system.
It started early April 2022.

The itch to put together enough assembly and a little C
to have a simple process running in a emulator or computer.

Might even be able to make a bootable usb stick!
Update mid-May: We did put it on a usb stick and it did boot on our old laptop!

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

Fast forward to mid-May, we have a small kernel, memory manager, kernel heap, screen and keyboard 
drivers, timer and real time clock, serial port logging, and a preemptive 
multi processing functionality with multi-priority ready queues, blocking semaphores, 
sleeping, yielding and exiting capabilities.

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

* a dynamic list of things. helps almost everywhere.
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

* Fix bug of 0 bytes malloc() from tty manager (i think the logger tty)
* Give console pci ability to probe and report on specific bus/device/func
* Make a good buffer class, with memory storage, with string operations
  that make sure we never write outside our memory area
  (e.g. insert at point, remove at point, etc) for various operations,
  allow two separate grow strategies: realloc() and trim (e.g. lose start)
  one of which is the TTY contents, another is the memory log. 
  Another can be ext2fs directories etc.
* If we have dictionaries and lists, we could easily map various devices
  (e.g. cpu, memory) with lists of subdevices etc, each with their properties.
* This virtual [display.h](https://github.com/levex/osdev/blob/bba025f8cfced6ad1addc625aaf9dab8fa7aef80/include/display.h#L25) is a good idea.
* Introduce [module names](https://github.com/levex/osdev/blob/bba025f8cfced6ad1addc625aaf9dab8fa7aef80/include/display.h#L25) in logging
* implement writing to SATA driver, clean up SATA code
* implement e2fs filesystem driver, initially RO, then RW on the image we have.
* make the build system rebuild the image, make this image our HDD solution for now.
* find out why the ProBook did not detect the 0/3/2 HDD devices, fix, write driver for them.
  fix it, so that we identify / start using those disks.
* take a look at levex/osdev in github for good ideas and PCI/ATA/EXT2 drivers
* good implementation of a circular buffer, with both reading and writing pointers
* good implementation of a list, similar to linux kernel, [here](https://kernel.org/doc/html/latest/core-api/kernel-api.html#list-management-functions)
* improve PCI devices, at least allow class/subclass names, to see what drivers 
we should write. USB might be a good first.
* start a tree-enabled device manager. ttys will need to be devices,
pci devices will need to be devices etc.
* move utility functions to libc/libk, make kernel use it.
* ata disk driver (detect, implement R/W operations) (info [one](http://www.osdever.net/tutorials/view/lba-hdd-access-via-pio), [two](https://wiki.osdev.org/ATA_PIO_Mode))
* the idea of an expandable array of things, similar to the approach of a StringBuilder,
something sile the one described [here](https://tiswww.case.edu/php/chet/readline/history.html#SEC6).
Essentially, allow add, get, index, remove operations without fear of running out, or without 
prior allocation.
* Write something about how to get to the arch specific build tools (some page in OSDev, i think [this one](https://wiki.osdev.org/GCC_Cross-Compiler))
* Basic libc functionality [more](https://wiki.osdev.org/Creating_a_C_Library)
* put konsole in a task, maybe allow it to own a tty device
* implement IPC using `send(target, message)`, `receive(target, &message)`, `sendrec()` and `notify()`. See "synchronous message passing" [here](http://www.brokenthorn.com/Resources/OSDev25.html)
* load and execute executables
* messages between tasks (IPC)
* file system driver (read / write some filesystem)
* libc for usermode
* porting a compiler for our os (maybe [this](https://wiki.osdev.org/Porting_GCC_to_your_OS) helps)
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
    * have a low level hotkey (e.g. Ctrl+Alt+Shift+M) to toggle some area of the screen to be used for kernel monitoring or console
    * maybe it can have pages, left and right or something like that
    * programs should not even be aware of it
* a random number generator ([example](https://wiki.osdev.org/Random_Number_Generator))

### things done

* improve the headers in the kernel, put them in a single folder, 
with whatever public interface they will contain.
* organize folders, especially process could be a subfolder inside kernel, with a single public header and mutliple internal headers and c files
* fix readline() to have many instances, not only one
* improve screen experience, by implementing scrolling back
* a few virtual terminals (e.g. a way of switching between two of them,
each having virtual screen buffers, scrollback, and switching wakes or sleeps each of them)
Maybe running already something on them (e.g. one for kernel log, one for konsole etc)
* something akin to gnu readline library, using just a getch()/putch() interface
* make logging having levels (DEBUG, INFO, WARN, ERROR, PANIC) and 
allow for setting up levels per destination.
* pci discovery, towards hdd and usb
* readline with history, and autocomplete (see [here](https://tiswww.case.edu/php/chet/readline/readline.html))
* makefiles for current organization, pages [here](https://wiki.osdev.org/Makefile) and [here](https://wiki.osdev.org/User:Solar/Makefile) for multiple binaries and modules
* semaphores with tasks sleeping and waking (from [here](https://wiki.osdev.org/Brendan%27s_Multi-tasking_Tutorial#Step_5:_Race_Conditions_and_Locking_Version_1))
* own repository
* document with brief paragraph describing kernel's concerns (e.g. memory, keyboard, screen, 
locking, task switching, idt, gdt, boot info, etc)
* kernel memory manager (kmalloc, kfree) - then update mutlitask.c to use dynamic memory
* scheduler
* multi tasking
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

### Dreamland wish list (push the envelope)

* Multiple entry points in a task or app, similarly to how Android is doing. Maybe this is something similar to Unix's signals.
* Something similar to Google Cloud functions: implement a small thing, configure when it is to be called (e.g. when a message of x becomes available, or when we boot or etc etc) and have it run.
* Kafka pipelines emulation would be easily implementable with shell pipes.
* Better handling of individual chunks of information, e.g. json objects with headers. Instead of just reading one stream of bytes, read a stream of those objects.
* If we can have an editor and a compiler, edit, compile and run software on this new os, it will be amazing!



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
* [BrokenThorn OS Development](http://www.brokenthorn.com/Resources/OSDevIndex.html), a collection of pages with nice and thorough explanatory information.

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
* a nice description of the concepts of linux (e.g. IPC) is located [here](https://tldp.org/LDP/tlk/tlk-toc.html)
* another nice repository with good libc implementation is [here](https://github.com/system76/coreboot)
* the whole [SBUnix](https://github.com/rajesh5310/SBUnix) is really nice!
* nice tutorial for the basic GUI framework for windows [here](http://www.osdever.net/tutorials/view/gui-development) - for when we get bored of the black console screen!



