# roadmap

* Finalize sfs tests
* Make all usertests pass, expand them
* Shape up the shell a bit, so it works ok
* Minimum editor, edit files, persist from session to session

## idea

Maybe make interfaces within the kernel? Things that code can depend on?
An output stream, or a logger, or a memory allocator, etc.



## still missing

* IPC
* signals & handling them
* soft links on filesystem
* devfs

Then the following efforts are possible

* Graphics environment (user-land compositor, IPC, shared mem)
* VT100-emulator, and pty devices, in order to create console windows.
* Porting of a small shell like `sash`, or `dash` if we prefer more posix
* Porting of a small C compiler, such as `tcc` which can create ELF executables
* Porting of a small editor, such as `kilo`, `pico`, `nano`, early `elvis` etc
* Porting of `bmake`, BSD's portable make
* Porting of `busybox` (requires posix affinity, provides about 50 tools for a usable system, including shell, editor, etc) - this would create an already usable system
* Bring in and port my interpreter, make it a default for scripts etc.
* Networking using ethernet
* If graphics + network, port a small browser, e.g. `NetSurf`
* Make the system self-sustained, compile kernel et all with bmake and tcc, tools for selection at boot.

Target set of commands:

* ls, cp, mv, mkdir, rmdir, touch
* echo, grep, cat
* clear, stty
* make
* true, false
* ps, free, top, etc
* tar, hexdump
* kill, reboot (through init)
* man (!!!)


## far future, possible library with

* high level programming functionalities (entities, persistence, events)
* Primitives: strings, numbers, booleans, blobs.
* Containers: hashtables, maps, lists, queues, tries, trees, etc.
* Predicates & functional approach
* Parsing/formatting json, yaml
* Key/Value store engine


## completed

* ~~Lots of debugging functionality, until we connected vscode debugger~~
* ~~Make the sfs_tool to generate image~~
* ~~Make and verify: fork(), execve(), spawn(), wait(), waitpid(), _exit(), elf loader, syscalls~~
* ~~Make and put the tiniest executables, into the image~~
* ~~Then bring in libc, on top of syscalls, make unit tests~~
* ~~Then bring in the basic programs from : init, shell, edit, etc.~~
* ~~kernel/libc/uapi --> libc/include/kernel --> rootfs/include/kernel~~

* ~~porting sash~~
  * ~~merge branch to main~~
  * ~~fix wait(), waitpid() and read() to not use EAGAIN~~
  * ~~continue with keyboard input, now that we can unblock~~
  * ~~implement readdir()~~
  * ~~keyboard driver + input~~
  * ~~implement O_CREATE etc~~
  * ~~implement attributes, owners, groups~~

* ~~Move from nursery to root, move root to graveyard~~


# Abstractions to target at some point

* Logger & appender
* Physical memory pages pool (with reference counter)
* Memory region - mappable, assignable, copiable, allocatable, releasable, extendable, etc
* Random sized heap allocator
* Output text stream (for debug, etc)
* Block device (disks, partitions)
* Screen (output, scroll, color, cursor, etc)
* File (open/read/write/close) - composability of UI, processes etc?
