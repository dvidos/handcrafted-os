# Handcrafted OS

An attempt to build a handcrafted operating system from scratch. It started early April 2022.

Inspiration was a game video. I loved C from my early days of embedded programming. 
I was curious about all the details.

Initially reading articles here and there, ended up heavily using [osdev.org](http://osdev.org/), minix and linux source code.


## Brief history

Early April 2022 I wrote a primitive bootloader in assembly. It would enter protected mode and could load additional sectors from disk. Upon trying to link a C generated primitive kernel, I stumbled upon the reality world of i386 vs i686 cross compilation issues.

Fast forward a few months, a C kernel evolved. It supported screen and keyboard interaction, a basic memory heap, basic multitasking capabilities. 

Forward a few more months and file system support came next. We chose to implement a FAT as the first file system.
With it, the ability for `open()`, `read()` from applications came, 
as well as the ability for shell to load and execute a user program.

The next frontier is either graphics mode (need a BIOS functionality for this) or networking.

## Goals

* To learn and implement how processes communicate with each other (IPC)
* To learn and implement how processes communicate with the kernel (ABI)
* To learn how multitasking is implemented, implement at least one scheduler
* To ultimate goal would be to have a shell, edit a file, compile and run.


## Kernel's architecture

Out of simplicity, I gravitated towards a monolithic kernel, before I got familiar with
the differences between microkernels and monolithic kernels.

There's quite a few parts at play that are being developed:

* The boot loader (assembly code to load and execute the kernel)
* The kernel itself, mainly consisting of the following:
    * Device drivers (e.g. PCI, clock etc)
    * Memory manager (to offer dynamic memory allocation)
    * File system interface (to offer the files abstraction over disks)
    * Multi processing (to allow many processes to run at the same time)
* A C runtime library (e.g. implementation of `open()` and `read()`)
* Various commands and programs to be executed (init, a shell, an editor, grep, etc)

More architectural notes in the file [architecture.md](docs/architecture.md)


## Folder organization

The main folders participating in this project are the following:

* `docs` - various documents about this project
* `src` - contais the source code of various artifacts
* `sysroot` - contains a folder structure for generating a disk image
* `imgs` - various disk images, used to test filesystems code

The source code folder (`src`) contains the following:

* `kernel` - contains the kernel, split into loader (assembly) and core (C)
* `libc` - contains code to generate both a kernel and a user C runtime library
* `user` - contains code for various utilities and programs


## Building this project

I develop and build this project on a Linux laptop. A cross compiler is needed, 
along with a few more tools. To set up the environment, I followed [this page](https://wiki.osdev.org/GCC_Cross-Compiler).

There are three main code folders, each with their own makefiles: the kernel, the C library, 
user utilities and programs. The building process involves packaging everything in a disk
image. To facilitate the process, the following scripts exist:

* `build.sh` - builds all the executables
* `iso.sh` - builds an iso image with a bootloader, the kernel and user files
* `emu.sh` - runs the qemu emulator with the built artifacts
* `clean.sh` - removes all temporary files


## Sources of reference

* [OSDev.org](https://wiki.osdev.org/Main_Page) for the most part
* Minix1 (mostly for an 8086) ([github repo](https://github.com/gdevic/minix1))
* Minix3 (for 386) ([github repo](https://github.com/Stichting-MINIX-Research-Foundation/minix))
* The Lions Book (unix v6) ([online pdf](http://worrydream.com/refs/Lions%20-%20A%20Commentary%20on%20the%20Sixth%20Edition%20UNIX%20Operating%20System.pdf), [online code](http://www.v6.cuzuco.com/v6.pdf))
* Linux 0.01 ([github repo](https://github.com/zavg/linux-0.01))
* Linux Kernel Map (though much modern and complex) [link](https://makelinux.github.io/kernel/map/)
* MIT's course OS xv6, for a RISK machine ([pdf](https://pdos.csail.mit.edu/6.828/2021/xv6/book-riscv-rev2.pdf), [code repo](https://github.com/mit-pdos/xv6-riscv)). An abandonned x86 version is [here](https://github.com/mit-pdos/xv6-public)
* [The Little book about OS development](https://littleosbook.github.io/)
* [Intel 80386 Programmer's Reference Manual](https://logix.cz/michal/doc/i386/), an unofficial copy with nicely explained, low level information and details.
* [BrokenThorn OS Development](http://www.brokenthorn.com/Resources/OSDevIndex.html), a collection of pages with nice and thorough explanatory information.
* [Github Showcases Open Source Operating Systems](https://github.com/topics/operating-system)
* [James' kernel dev tutorials](http://www.jamesmolloy.co.uk/tutorial_html/)