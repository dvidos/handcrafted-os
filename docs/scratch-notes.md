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


## various unrelated notes

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



## on C quirks

We resorted to `.cinl` files with C source code to be included in a single `.c` file.
This was to allow lots of `static` methods, allowing hiding of implementation and some
data structures. An example is the `filesys/fat` folder, with FAT16/32 code.

This requires a careful dependency discovery, through the `-MD` switch of `gcc`, which
generates `.d` files, with all the dependencies of a particular object file.

