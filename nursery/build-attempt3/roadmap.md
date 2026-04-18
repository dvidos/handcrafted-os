# roadmap

At high level, the steps will be:

* ~~Make the sfs_tool to generate image~~
* ~~Make and verify: fork(), execve(), spawn(), wait(), waitpid(), _exit(), elf loader, syscalls~~
* ~~Make and put the tiniest executables, into the image~~
* ~~Then bring in libc, on top of syscalls, make unit tests~~
* ~~Then bring in the basic programs from : init, shell, edit, etc.~~
* ~~kernel/libc/uapi --> libc/include/kernel --> rootfs/include/kernel~~

## porting sash to work as shell
  * ~~merge branch to main~~
  * ~~fix wait(), waitpid() and read() to not use EAGAIN~~
  * ~~continue with keyboard input, now that we can unblock~~

  * keyboard driver + input
  * implement Kill, signal
  * ~~implement readdir()~~
  * implement O_CREATE etc
  * implement attributes, owners, groups
  * implement symlinks, readlink, symlink, lstat etc


* Move from nursery to root, move root to graveyard
* Create a filter in userapps for single-file programs, make appropriate makefile
  * Create true, false, cat, and all those small things we need


_(we are now in parity with existing kernel, move to project root)_

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


In the future, possible library with:

* Primitives: strings, numbers, booleans, blobs.
* Containers: hashtables, maps, lists, queues, tries, trees, etc.
* Predicates & functional approach
* Parsing/formatting json, yaml
* Key/Value store engine
