# roadmap


## debugging improvements

See chat with [gemini here](https://gemini.google.com/app/e41922c6044c3e03)

* implement addresses + symbols array
* implement backtrace() (EBP hopping + symbol names)
* improve panic() with a backtrace
* create unit tests using assert() / panic(), `make run-tests`
* improve page fault handler, with better visibility
* interactive kernel shell, in COM1, where logs go to COM2
  * ability to print values by address, based on formatters

e.g. 

* Dual UART (COM1/COM2) initialization.
* ~~i686 EBP-chain backtrace.~~
* ~~Two-pass Linker script for Symbol/Data mapping.~~
* pp (Pretty Print) Registry for kernel objects.


## still missing

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
