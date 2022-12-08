
# second stage boot loader

Essentially to be loaded by the small boot sector code

Can be any number of sectors long (e.g. can easily be 16 KB).
Planning to load it on 0x1000 (64KB).

Needs to be a mix of assembly and C (though it's difficult to emit real mode C). 
Also, our linked package should be fairly simple, e.g. no ELF headers.
Data segment could be after code if we wanted to.
The `start` function must be the first code in the first section of the image, 
so that our first stage boot loader can jump to it.

Needs to be stored at a known point in the disk (sector based, not file system based)

See the following articles:

* https://wiki.osdev.org/Rolling_Your_Own_Bootloader
* https://wiki.osdev.org/BIOS
* https://wiki.osdev.org/My_Bootloader_Does_Not_Work
* https://blog.ghaiklor.com/2017/11/02/how-to-implement-a-second-stage-boot-loader/
* https://dc0d32.blogspot.com/2010/06/real-mode-in-c-with-gcc-writing.html
* http://www.cs.cmu.edu/~410-s07/p4/p4-boot.pdf

It has to do the following things, roughly:

* Get information from BIOS (e.g. memory sizes)
* Probe and maybe enter graphics mode
* Parse filesystem (FAT? ext2?) to find the `/boot/kernel` file
* Parse elf header and load the kernel on 0x100000 (1MB)
* Prepare a multiboot2 environment and run the kernel.

Memory organization

```
+--------------------+
| top of stack       |  0xFFF0 (64 KB)
|   ...              |     (33 kb space)
| boot sector code   |  0x7C00
|   ...              |     (23 kb space)
| 2nd stage          |  0x2000
| boot loader code   |
|   ...              |     (8 kb space)
| interrupts table   |  0x0000
+--------------------+
```

