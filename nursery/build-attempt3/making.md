
# making the project

* To make the whole project, run `make`
* To run in emulator, run `make run`
* To cleanup temp files, run `make clean`

The script `configure.sh` is calculating sizes and is creating the `config.mk` that is included in all makefiles.

The main product will the `os.img` image file, in this folder. 🔥
It will contain the bootloaders, the kernel, and a populated file system.

The date and git hash will be baked into the built kernel, shown at boot time, and available through a syscall. 🚀

You'll need a cross compiler, good luck! 🍀
