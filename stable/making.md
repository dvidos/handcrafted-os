
# making the project

* To make the whole project, run `make`
  * This produces the `os.img` file
* To run in emulator, run `make run`
  * This runs the `qemu` emulator
* To cleanup temp files, run `make clean`
  * This removes build artifacts leaving clean code.

The script `tools/configure.sh` is calculating sizes and is creating the files `config.inc.mk`, `config.inc.sh` and `config.inc.h`, that are included in makefiles, scripts, source.

The main product will the `os.img` image file, in this folder.
It will contain the bootloaders, the kernel, and a populated file system.

The date and git hash will be baked into the built kernel, shown at boot time, and available through a syscall.

You'll need a cross compiler, good luck!


## sequence & dependencies

Since some components depend on others, `make install` cannot be a single step.
Each step may copy things to the build directory.

* `stage1.bin` built from assembly
* `stage2.bin` built from assembly and C
* `kernel.bin`
  * built from C and some assembly
  * copies the public headers (uapi) to build directory
* `libc.a`
  * built from C and some assembly
  * uses headers: internal, public ones, and the kernel's ones in build directory
  * copies public headers to build directory
* `userapps`
  * built from C
  * they use headers and libc in build directory


