
# making the project

* To make the whole project, run `make`
  * This produces the `os.img` file
* To run in emulator, run `make run`
  * This runs the `qemu` emulator
* To cleanup temp files, run `make clean`
  * This removes build artifacts leaving clean code.



The script `tools/configure.sh` is calculating sizes and is creating the files `config.inc.mk`, `config.inc.sh` and `config.inc.h`, that are included in makefiles, scripts, source.

The main product will the `os.img` image file, in this folder. 🔥
It will contain the bootloaders, the kernel, and a populated file system.

The date and git hash will be baked into the built kernel, shown at boot time, and available through a syscall. 🚀

You'll need a cross compiler, good luck! 🍀
