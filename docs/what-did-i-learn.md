
# What did i learn (from writing an OS)

Despite this being a not finished or polished effort (there's a ton of things to expand to)
I feel I learned the basics I set out to learn. 
I could expand a bit on networking, maybe in the future.

I wanted to write down all the things I learned, for two reasons:

1. to have them in a concise format, should anyone (or my future self) care
2. to maybe have them as a reference for a future possible rewrite
3. I will have forgotten everything in a year anyway!

Project started April 2022, this file written Nov 2022.

## The big pieces of an operating system

Very roughly, the big pieces of an operating system include the following:

* Devices abstraction and management (e.g. screen, keyboard, disks, etc)
* Process management (start, switch, fork, etc)
* Filesystem implementation (mounts, file operations etc)
* Memory management (ability to allocate memory for specific virtual address)
* Offer a host of functionality to user programs, through `syscall()`


## The boot sequence

What I ended up as a boot sequence is summarized below:

The BIOS, either through the legacy booting system, or the UEFI, 
finds a and loads a sector of the boot loader.

The bootloader is really small in size (less than 510 bytes), so it can only
load a small program, which is the second stage loader.

The second stage loader has specific filesystem code, so it can mount
and read a partition, to find the kernel image to load.
After loading it, it jumps there.

The kernel runs various hardware initializations,
then spawns the user processes. 
In this OS, the user process is simply a shell,
there is no login, and no init scripts (yet)

### Inside the kernel

The main kernel does the following:

1. Start an in-memory logging buffer (to log messages)
2. Initialize the VGA screen driver (to show booting messages)
3. Save any possible multiboot information from the boot loader (e.g. the memory map)
4. Initialize the Global Descriptor Table (to setup 4 segments, 2 code, 2 data, 2 for kernel, 2 for user space)
5. Initialize the Interrupts Descriptor Table (by offering interrupt hooks)
6. Initialize the Programmable Interrupt Controller (shifting interrupts to the 0x20+ range)
7. Initialize the Programmable Interval Timer (to generate an interrupt every millisecond)
8. Initialize the Real Time clock (to allow us to get the date and time of day)
9. Initialize the Physical Memory Manager (it knows what memory is usable or reserved (e.g. mem mapped IO) and allows allocation and deallocation of pages)
10. Initialize the Kernel Heap (to allow dynamic memory, e.g. for the filesystem)
11. Initialize the Virtual Memory Paging (mapped memory access, with different page directories for each process)
12. Enable interrupts (this allows keyboard and timer to work)
13. Detect PCI devices (e.g. can detect SATA disks or network cards)
14. Initialize a RAM disk (more for experimental use and tests, really...)
15. Initialize the file system (e.g. detect partitions in disks, discover existing filesystems, mount root and possibly others)
17. Initialize TTY pseudo-terminals (to allow each process to have its own terminal)
18. Prepare the first tasks (e.g. shell launcher and kernel monitor) and start multitasking.

This is the end of the kernel's `main()` function. 
The various processes will run for the rest of the uptime of the computer.

### Memory organizartion

The various segments of the kernel follow, one after the other.

1. Code segment, loaded at 1 MB. Roughly 100 KB long. 
2. RO data segment, usually strings. Usually 1 KB long.
3. Initialized data segment. small in my case.
4. Uninitialized data segment. Pretty big if we have big bitmaps e.g. for memory management.
5. Stack of kernel. I think mine is 64 KB deep. Keep in mind, in calls form programs, we are in the program stack context.
6. The kernel heap. Currently 4MB, set at compile time.
7. The ramdisk. In my case 4MB.

The use programs are usually loaded in the 128MB mark. This is because 
the entry point for ELF executables is set at compile time at a little after 0x8048000.
The stack is set below that, to guard against underflow, and the heap is set 
after the various data segments, to allow it to grow, using `sbrk()`

## Hardware abstraction

This is the first and most important property of an operating system.
Instead of dealing with ports and memory mapped I/O, 
the OS offers an abstraction over them.

Examples:

* Screen becomes a collection of selectable, scrollable terminals
* Keyboard becomes a queue of events, where one can poll
* The timer chip becomes a mechanism to provide a monotonously increasing value

I really adore this property of any system, taking something 
and tranforming it to something else: 

* A 2K video mapped memory area becomes multiple terminals,
or windows with hosted applications. 
* A key interrupt with 
some scan codes becomes a letter and number entry, 
shift key combinations, multiple languages support,
distributing key presses in multiple event queues,
depending on the currently selected application.
* A flat disk becomes a tree of directories, where 
one can create and resize files on demand.

etc.

## Processes

Given the support for a timer interrupt, we can implement the idea of multiple processes.
Essentially, every some time (say 50 msecs), during a timer interrupt,
we can mess up with the address that the interrupt will return to.
By switching the stack frame, we fool the CPU to return to a different
address.

This very old technique (at least since the days of Multics) allows
multi processing on our machines. The Apollo AGC computers supported
up to 7 simultaneous jobs, using cooperative multitasking. 
This was during the 60s.

Around this basic functionality of multi processing, the various 
functionalites evolved:

* `sleep()` for a number of milliseconds. Allows for waiting the time to pass,
without using up resources. Imagine implementing a cron server in a loop.
* `fork()` or other equivalent way to start a new processes. 
* `exit()` to allow the process to exit, signaling a specific exit code.
* `wait()` to wait for a process to exit, getting its exit code.
* `yield()` to cooperatively give the CPU to another process.
* `schedule()` is called from the timer interrupt, to pick a different task to run for a while.

There is a small discussion there on how to pick the next process to run.
There's lots of algorithms one can pick. The simplest we picked was an array 
of 5 different priorities, each having a list of runnable processes.
We would round robin the processes, by prefering the higher priorities first.

An interesting thing is the way various blockings are implemented.
Whenever a process is to "wait" on something, it essentiually is 
taken out of the runnable processes ring, and put on a special queue to wait.
An example is listening for a message, waiting for user input, waiting for a child to exit etc.

On the other hand, whenever the event we wait for happens, we need to go 
over the wait lists and unblock any process that waited for this.

From the process perspective, the process called a simple method such as `sleep()`, `wait()`
or `getkey()`, and it got the answer. The fact that the call may have lasted 
many seconds (or even a whole night, through a suspend cycle) is totally transparent
and unknown to the application. Only the OS knows what really happened.

## Filesystem

The root point being a memory reference of a root directory somewhere in some partition.

The mounting table, having pairs of directory pointers for each mount, except the first.

To open a file, follow the tree to directories, finding entries, jumping across mounted filesystems/

When opening, the support of multiple file systems (e.g. `FAT` or `ext2`) and multiple pointers to functions.

When writing, how to cache and write only dirty sectors upon close

In theory, how to check if a directory can be deleted, when some other program has it opened or mounted.

## Loading and executing user programs

Something I always found fascinating was loading and executing a program.
I could not believe how my code would get loaded into memory and executed.
Turns out it's simple enough (though a bit more complicated!)

Parse ELF, various blocks, code, data etc.

Prepare stack

Load into memory, using paging to ensure specific code addresses.

Currently the addresses we load the program into are the following:



Call initialization of libc (memory etc), then call the `_start()` method, which calls `main()`.

How environment and arguments are passed in.

How heap is initialized and maybe expanded.

How we cannot load the executable before we switch CR3 (the page directory register),
so we create a small task to first load and then execute the executable.

How we cannot remove the process from the running list during the call to `exit()`
What we do is clean up the process in the next `idle()` task execution.

Oh, keep in mind, the kernel space is always mapped as well for each page directory 
we prepare, e.g. each program feels it is the only one loaded into memory 
and the kernel is always loaded there as well.

## Networking

Did not do much here, mainly due to lack of knowledge about how to write drivers
for a network card, or WiFi adapter, as is the usual case today.


## libc

The idea of having to implement all / most functions (e.g. `memcmp()` or `strcpy()`).
Then compile all your programs with the specific libc.

The idea that system specific functions (such as `sleep()` or `getkey()`) are essentially
hooks into the OS code, usually through an interrupt.

## Sidebar: takeaways

* `for(;;) asm("hlt");` was our friend for a long time...
* It was a huge help, when we implemented `printf()`
* It was also a huge help, when we implemented a good logging system (module names, log level, various appenders)
* Memory management could be better. We did not performance measure our `malloc()` algorithm
* Working with interrupts, especially around context switching, was a slow process. 
The alternative was to learn assembly and read the Intel Programming Manual, and that would be even slower.
* Don't use any local variables in the switching function.
* I aspired to remove things, to end up with a minimal kernel, but there's just too many things it must do.
* Unit tests are the best, still I wrote too few...
* Gotten used to calling methods that never return.
* Gotten used to having cleanup sections in functions, using `goto` to clean and exit.
* C's lack of namespaces or other way to hide things, but the `static` in a single file, is pretty limiting...

## Sidebar II: If I rewrote it (or heavily refactory it)

* Into graphics mode early on. More pleasant to work with.
* Implement networking code.
* Find a shell to port, it's too time consuming to write all this parsing / scripting code.
* Find a vi to port, it's too time consuming to write all these buffer operations.

