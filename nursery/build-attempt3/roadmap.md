# roadmap

At high level, the steps will be:

* ~~Make the sfs_tool to generate image~~
* ~~Make and verify: fork(), execve(), spawn(), wait(), waitpid(), _exit(), elf loader, syscalls~~
* ~~Make and put the tiniest executables, into the image~~
* ~~Then bring in libc, on top of syscalls, make unit tests~~
* Then bring in the basic programs from : init, shell, edit, etc.


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

