# things to do

* Implement a "make test" functionality for kernel as well.
Move the testing framework to the library and make a few tests
to ensure usefulness.

* Maybe some very lean stack/queue/list implementations in a macro,
similar to [this one](https://github.com/huawenyu/Design-Patterns-in-C/blob/master/auto-gen/util/stack.h), for example for use in the testing framework lists.

* Start implementing a String class, to be used widely in many cases.

* Then derive a way to inject memory and disk dependencies into modules,
for testing, e.g. for the FAT or ext2 filesystems or the memory allocator.
Could be something like the global function pointers that data structures have.
Get to a point where we are thoroughly unit testing the FAT code.

* If I want to avoid grub and rely on my code 100%,
  * Implement a 2nd stage loader on sectors 2-31 of disk. This allows for 15 KB loader
  * Implement a data area of .. some KB for any information I want to load (e.g. fonts)
  * Make a tool that allows to create / modify this image (maybe `dd` & `cat` is enough though)
  * Update the FAT to have reserved sectors for this.
  * Extend the img.sh script to allow create the image file by copying the two bootloaders, then creating a FAT with 10 reserved sectors
  * 1st boot loader at 0x7C00 = ~ 32K
  * We can load the 2nd stage boot loader at 0x10000 = 64K, we'll have 512K - 64K = 400K for our code
  * The 2nd stage will run in real mode
  * It will collect memory info, graphics info, get into graphics mode, load the kernel, load any additional data files (initrd?)
  * Finally, it will initialize protected mode and jump to the kernel, passing multiboot and ramdisk info.
  * See https://wiki.osdev.org/Rolling_Your_Own_Bootloader


* Improve the making process, by targeting a disk image that will contain everything.
The boot sector, the kernel loader, the kernel, the user utilities.

* find a way to have: output of one program can be the input to another.
that is the unix philosophy. But can also allow us to assemble systems.
For example we can have filters, routers, distributors, aggregators, etc.
Not only for ascii data and line-by-line, but also json objects.


* write learnings from this project, what threads we had open, and close for the season.

* make malloc uniform, it's bad to have two systems.
  * make it support some initialization, so we can initialize from either _start() or from kernel's main.
  * it will help us discover dependencies, like sbrk(), or log() etc.
  * We could have malloc depend on initial address and a sbrk() call, where in kernel, 
  it would call the page allocator, and in user land, it would call sbrk() via syscall.

* validate mkdir, rmdir, touch, unlink work
* make all fat code to use the sector and cluster of superblock. clean up local files and clusters.
* try to organize / write low level routines that make sense for fat. allocation, clusters, chains, etc.
* improve the ram disk code and try to make a unit test for fat.

* write a doc how the VFS works
  * include: storage devices, partitions/logicalvols, filesystems
  * include: how to write a file system (e.g. FAT or ext2)
  * include: how to write a storage device driver (e.g. USB stick or loop device)
  * include: a simple open/read/close program and how calls work

* fix file things:
  * copying of file_t data in handles in process space.
  * remove old logic of opening based on dentry
  * ability to work with cwd and root (resolve absolute && relative paths)
  * file manipulation in shell (cat, mv, cp, rm)
  * given an opened directory handle, find and return a dir_entry, based on a path.
  * [ok] go and rename all "file_system" names into "filesys" for brevity
  * go and typedef all "struct storage_dev" etc into "_t" types
  * move all vfs structures into their own files (entry, file etc)

* When VFS works again, implement some flags on open()

* Research, adopt & implement qsort(), bsearch(), lsearch(), hsearch() and tsearch().
  * In theory binary search, linear, hash, tree.

* shell command for setting error levels per appender and per module
* fork(), seems to be a nice challenge.
* Improve build system to create and run a single .img file, without the .iso (see [here](https://github.com/stevej/osdev/blob/master/image-builder/create-image.sh))
* Shell, working dir, chdir, file operations (cp, mv, cat)
* Resume the vi editor, get to editing some files
* Good generic and simple list implementation [here](https://github.com/stevej/osdev/blob/master/kernel/include/list.h). Maybe we want to use it. Oh, he also has a nice [tree implementation](https://github.com/stevej/osdev/blob/master/kernel/include/tree.h)
* regarding lists like environment variables or arguments etc, play with the idea
of allocating small arrays of pointers, but nest them, the way ext2 is doing, to achieve
10, 100, 1K of entries etc. Allow infinite upper limit, but with small allocation for a dozen items.
* Create a memory disk storage device, use it to unit test the FAT16 & FAT32 drivers
* Start a shell and a vi editor
* Make one console command that allows us to read/modify/write sectors off of hard disk
* Maybe I want to get fast to a place where I can boot the OS and 
and have an environment where I can improve it, e.g. test and compile apps
or something.
* Actually eating own dogfood can be the ultimate goal. Actually use if for day to day operations...
* Sounds like implementing TCP/IP will lead to exciting capabilities.
We may need a wifi or ethernet driver for that...
* Improve malloc() in the K&R style,
  * by keeping a list in the free blocks only
  * by wrapping the to-be-returned block with only one magic byte at each side
  * by being able to ask for more memory from the system (even non-continuous)
  * by merging freed blocks with possible adjacent free blocks
  * see <https://stackoverflow.com/questions/13159564/explain-this-implementation-of-malloc-from-the-kr-book>
* Possibly implement a full ext2 file system
* Finalize the small vi editor
* Make physical memory manager to be two step allocation system:
  * a 4KB page contains 32K bits and can track 32K pages
  * A 4 GB system consists of 1M pages. To track them, we need 32 4KB pages (128 Kb).
  * When allocating the pages, we mark 32 of them as busy and reuse them for tracking the rest
  * If we have an array of 32 long ints, we can track their address or page num,
  * Then we need only one 32 bit number to mark which tracking pages contain free pages!
  * Make sure any pages we use for tracking the physical memory are always mapped 1:1 in the virtual memory mapper.
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
* good implementation of a circular buffer, with both reading and writing pointers
* good implementation of a list, similar to linux kernel, [here](https://kernel.org/doc/html/latest/core-api/kernel-api.html#list-management-functions)
* improve PCI devices, at least allow class/subclass names, to see what drivers
we should write. USB might be a good first.
* start a tree-enabled device manager. ttys will need to be devices,
pci devices will need to be devices etc.
* move utility functions to libc/libk, make kernel use it.
* the idea of an expandable array of things, similar to the approach of a StringBuilder,
something sile the one described [here](https://tiswww.case.edu/php/chet/readline/history.html#SEC6).
Essentially, allow add, get, index, remove operations without fear of running out, or without
prior allocation.
* implement IPC using `send(target, message)`, `receive(target, &message)`, `sendrec()` and `notify()`. See "synchronous message passing" [here](http://www.brokenthorn.com/Resources/OSDev25.html)
* messages between tasks (IPC) - maybe shared memory ([this one?](https://github.com/stevej/osdev/blob/master/kernel/include/shm.h)) or [this one](https://tldp.org/LDP/lki/lki-5.html#ss5.2)
* Mauch later, find way to detect very small timings, to fine tune malloc() for example.
* porting a compiler for our os (maybe [this](https://wiki.osdev.org/Porting_GCC_to_your_OS) helps)
* nice documentation about lists, semaphores, tasks, atomic operations [here](https://tldp.org/LDP/lki/lki-2.html#ss2.11)
* primitive tools (tiny shell, ls, cat, echo, tiny vi etc)
* greek language keyboard mapping and utf8 support, even in console
* the four main tasks that user mode code can use (also described in Minix Book)
  * process management (fork, exec, exit, etc)
  * I/O device management
  * memory management (malloc etc)
  * file management
* other topics
  * Beeper control for fun (maybe from PIT channel 2)
* sorting, hashing and other algorithmic implementations
* a man page system, for writing and displaying man pages (eg in usr/share/man/xxxxx.txt)
* consider our own boot loader
  * a 512 bytes boot sector, with a second stage loader
  * ability to put into graphics mode, detect memory, switch to protected mode
  * then call kernel
* [NetSurf](http://www.netsurf-browser.org/) seems to be a good browser we could port over.
* Instead of GLib ToaruOS uses the [newLib](https://sourceware.org/newlib/) which is more lightweight
* The ToaruOS guy also said that porting `gcc` over was easy, as all it needs is to read and write files.
* The ToaruOS also seems to have a nice integrated gui architecture with a windows approach similar to what we used for our TTYs.
* When we have what we think we want, remove as much as possible, up to the point we can remove nothing more.


## things done

* Monitor page with discovered storage devices and partitions, registered file systems, current mounts, and other stats...
* Make environment changeable in a process - also see this: https://stackoverflow.com/questions/64004206/where-are-environment-variables-of-a-process-is-stored-in-linux and this https://codebrowser.dev/glibc/glibc/stdlib/setenv.c.html
* Make argv[0] to point to executable name, not first argument
* Make logging include module name, allow turn on/off per module name
* syscall to get clock & uptime
* randseed() and rand() in libc (randdemo program as well)
* exec() and wait() system call allows parents to launch and wait for children
* File handles + operations per process
* kernel monitor - on tty4, shown in png captured
* Some syscalls that return process, devices and filesys information,
so we can create some monitor applications.
* Executing executable files from shell
* Supporting files (VFS) ops from user programs
* load and execute executables in their own processes, using exec()
* Basic libc functionality [more](https://wiki.osdev.org/Creating_a_C_Library)
* put konsole in a task, maybe allow it to own a tty device
* ata disk driver (detect, implement R/W operations) (info [one](http://www.osdever.net/tutorials/view/lba-hdd-access-via-pio), [two](https://wiki.osdev.org/ATA_PIO_Mode))
* file system driver (read / write some filesystem)
* libc for usermode
* user mode file system operations (open, read, write, close etc)
* Make more clear how the kernel loading works
  * get more variables / symbol addresses from linker script
  * make sure kernel heap starts right after kernel, so we can map it and extend it
* Implement writing in FAT filesystem
* SysCall, make everything supported, write libc functions
* Improve exec() by requiring virtual mapped memory for the executable.
* To overcome the problem of starting a task for an executable, think about the exec() starting a more primitive service, with only kernel space, then it will expand it and load the executable, prepare the long stack, and jump on the crt0 entry.
* Also, create a function, after `process_create()` to be named `process_cleanup()`. 
and call that function from the idle task, so that the cleanup code is near the allocation code.
* Make FAT read/write
* FAT system, write(), touch(), mkdir(), unlink() etc
* Implement virtual memory paging (with cr0 support)
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

* Could implement setjmp() / longjmp() in my C library, so that I can do
  exception handling in my C programs!


* If I could dream a lot, I would like to have processes that know to do one think well
(e.g. get a webpage, sort content, keep persistant store etc). The system would 
make sure to start and stop them, no user intervention would be needed. 
The user would just write links between them, like pipes or like excel references,
and have things work out. For example, get the weather and visualize the amount of snow
over alaska. The user would be able to edit / change them in real time, with immediate
feedback, without needing to stop / start etc. Essentially, hide all complexity 
from the user (starting or stopping services, save or load files etc) and 
allow them a constant tinkering of things to whatever they want to do.
Not in the Steve Jobs way, but in the Ken Thompson way.
Maybe we could do the same for the graphical interface. The same way 
that it is easy for processes to use a text output today, we could
have gui windows than can choose between text, 2d and 3d surfaces.
It should then be really easy to draw in a 2D surface, or draw 3D objects
and have them rendered correctly.

**Elastic memory management?**

* Visionary memory management: similar to disks,
instead of specific malloc() and free(), to have the ability 
to create segments of memory, allow them to grow or shrink at will,
and have them cleaned up later, maybe based on a bucket system or something.
* The blocks of memory chunks should be steady, to allow for pointers to work,
but reading/writing may need to be performed by functions, instead of memcpy(),
because the blocks will not be sequential.
* Unless, try to divide the memory into as big blocks as possible everytime,
to allow room to grow.
* Such thing would allow for dynamic strings, arrays, lists, collections, etc.
