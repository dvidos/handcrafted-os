

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


