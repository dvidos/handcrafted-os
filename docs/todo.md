# things to do


* Implement writing in FAT filesystem
* Possibly implement a full ext2 file system
* Finalize the small vi editor
* Implement virtual memory paging (with cr0 support)
* Make physical memory manager to be two step allocation system:
    * a 4KB page contains 32K bits and can track 32K pages
    * A 4 GB system consists of 1M pages. To track them, we need 32 4KB pages (128 Kb).
    * When allocating the pages, we mark 32 of them as busy and reuse them for tracking the rest
    * If we have an array of 32 long ints, we can track their address or page num,
    * Then we need only one 32 bit number to mark which tracking pages contain free pages!
* Fix bug FAT code to read more than 32 dir entries
* Fix bug of 0 bytes malloc() from tty manager (i think the logger tty)
* Improve the keyboard driver (see [here](http://www.brokenthorn.com/Resources/OSDev19.html))
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
* Basic libc functionality [more](https://wiki.osdev.org/Creating_a_C_Library)
* put konsole in a task, maybe allow it to own a tty device
* implement IPC using `send(target, message)`, `receive(target, &message)`, `sendrec()` and `notify()`. See "synchronous message passing" [here](http://www.brokenthorn.com/Resources/OSDev25.html)
* load and execute executables
* messages between tasks (IPC)
* file system driver (read / write some filesystem)
* libc for usermode
* porting a compiler for our os (maybe [this](https://wiki.osdev.org/Porting_GCC_to_your_OS) helps)
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
* sorting, hashing and other algorithmic implementations
* a man page system, for writing and displaying man pages (eg in usr/share/man/xxxxx.txt)

## things done

* Write something about how to get to the arch specific build tools (some page in OSDev, i think [this one](https://wiki.osdev.org/GCC_Cross-Compiler))
* loading and executing programs (ELF format), implement syscall (int 0x80) -- success July 2, 2022
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

## dreamland wish list (push the envelope to-do list)

* Multiple entry points in a task or app, similarly to how Android is doing. Maybe this is something similar to Unix's signals.
* Something similar to Google Cloud functions: implement a small thing, configure when it is to be called (e.g. when a message of x becomes available, or when we boot or etc etc) and have it run.
* Kafka pipelines emulation would be easily implementable with shell pipes.
* Better handling of individual chunks of information, e.g. json objects with headers. Instead of just reading one stream of bytes, read a stream of those objects.
* If we can have an editor and a compiler, edit, compile and run software on this new os, it will be amazing!

