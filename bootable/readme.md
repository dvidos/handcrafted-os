

# bootable

An attempt to build a bootable... something.

The itch to put together enough assembly and a little C 
to have a simple process running in a emulator or computer.

Might even be able to make a bootable usb stick!

Following this guy: https://dev.to/frosnerd/writing-my-own-boot-loader-3mld

## description

Essentially, we write 512 bytes of assembly, in real mode,
that are loaded into address 0x7c00 from the BIOS.

Those 512 bytes are used to load a bunch of disk sectors into memory,
again written in assembly, that will allow us to load a larger kernel.

Finally, the kernel, written in C, will kick things off.


## history

Early April 2022 I wrote a basic bootloader in assembly.
It would enter protected mode and could load additional 
sectors from disk. Upon trying to link a C generated 
primitive kernel, I stumbled upon the reality world of 
i386 vs i686 cross compilation issues.

Therefore I started reading http://wiki.osdev.org/


## things to implement (maybe in order)

* a way to launch and run tests suites (qemu passes arguments through the -append argument)
* std funcs, towards a library for user programs: 
    * mem\*
    * str\*
    * atoi, itoa, atol, ltoa
    * malloc/free
    * printf, puts, putchar
    * panic, assert
* text mode screen driver
* keyboard input
* scheduler and ability to start / stop processes
* primitive file system (maybe in memory for now, maybe ext2 later))
    * fopen, fread, fwrite, fseek, fclose
* a build script to bring all things together, since a makefile will not cut it (e.g. [minix3 build.sh](https://github.com/Stichting-MINIX-Research-Foundation/minix/blob/master/build.sh))

* primitive memory management (maybe not even virtual for now)
* some kind of initialization sequence (init, initrd, etc)


## goals

* learn and implement how processes communicate with each other
* learn and implement how processes communicate with the kernel
* learn how multitasking is implemented, implement at least one scheduler

ultimate goal ? maybe ability to edit a file, compile and run. though that's a long goal. maybe [smaller C](https://wiki.osdev.org/Smaller_C) is an answer

## architecture notes

It seems minix is a microkernel. There are separate tasks for memory management, filesystem, clock, etc.
Each time a process wants to do something, it will make a call (say, to malloc(), open(), fork(), sleep() etc).
This leads to a message being sent to the target task. The sender is blocked and put to sleep.
If the task is already handling some other message, the message is put in a queue.
Each task has a loop for receiving messages and acting on them. I think that's the module that insmod
is doing for linux as well.
When the task has been performed, the response is sent back to the original sender, via IPC
and the sender is awakened.







