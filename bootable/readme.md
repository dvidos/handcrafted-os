

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

------------------------------------------------------

## history

Early April 2022 I wrote a basic bootloader in assembly.
It would enter protected mode and could load additional 
sectors from disk. Upon trying to link a C generated 
primitive kernel, I stumbled upon the reality world of 
i386 vs i686 cross compilation issues.

Therefore I started reading http://wiki.osdev.org/



