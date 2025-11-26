# startup log

Here is a typical log from booting the kernel using Grub.

Main parts:

* Memory map detection, from linker's symbols
* Multiboot info, getting memory map from that
* Initializing the hardware subsystems
* Detecting storage devices
* Probing partitions and filesystems, mounting "/"
* Initializing multitasking, spawning a few shells

```
[0.000] MAIN INFO : C kernel started
[0.000] MAIN INFO :   Segments                     Size  From        To
[0.000] MAIN INFO :   code (.text)               104 KB  0x00100000  0x0011A13A
[0.000] MAIN INFO :   ro data (.rodata)            1 KB  0x0011A13A  0x0011B736
[0.000] MAIN INFO :   init data (.data)            0 KB  0x0011B736  0x00123328
[0.000] MAIN INFO :   zero data & stack (.bss)   155 KB  0x00123328  0x0014AD00
[0.000] MAIN INFO :   heap                      4096 KB  0x0014B000  0x0054B000
[0.000] MAIN INFO :   ramdisk                   4096 KB  0x0054B000  0x0094B000
[0.000] MAIN INFO : Bootloader info detected, copying it, size of 120 bytes
[0.000] MAIN INFO : Multiboot info
[0.000] MAIN INFO : - flags: 0x00001A67  (00000000000000000001101001100111)
[0.000] MAIN INFO : - memory lower 0x27F (639)
[0.000] MAIN INFO : - memory lower 0x1FFB80 (2096000)
[0.000] MAIN INFO : - root partition 0xE0FFFFFF (drive no, p1, p2, p3)
[0.000] MAIN INFO : - cmdline ""
[0.000] MAIN INFO : - ELF info provided
[0.000] MAIN INFO :   elf n 16, s 40, a 10138, i 15
[0.000] MAIN INFO : - mmap address 0x100A8, length 144
[0.000] MAIN INFO :   s 20,  a 0x00000000-00000000,  l 0x00000000-0009FC00,  t 1
[0.000] MAIN INFO :   s 20,  a 0x00000000-0009FC00,  l 0x00000000-00000400,  t 2
[0.000] MAIN INFO :   s 20,  a 0x00000000-000F0000,  l 0x00000000-00010000,  t 2
[0.000] MAIN INFO :   s 20,  a 0x00000000-00100000,  l 0x00000000-7FEE0000,  t 1
[0.000] MAIN INFO :   s 20,  a 0x00000000-7FFE0000,  l 0x00000000-00020000,  t 2
[0.000] MAIN INFO :   s 20,  a 0x00000000-FFFC0000,  l 0x00000000-00040000,  t 2
[0.000] MAIN INFO : - boot loader name "GRUB 2.06-2ubuntu7.2"
[0.000] MAIN INFO : - vbe control info  0x104A8
[0.000] MAIN INFO : - vbe mode info     0x106A8
[0.000] MAIN INFO : - vbe mode          0x3
[0.000] MAIN INFO : - vbe interface seg 0xFFFF
[0.000] MAIN INFO : - vbe interface off 0x6000
[0.000] MAIN INFO : - vbe interface len 0x4F
[0.000] MAIN INFO : - framebuffer addr   0x0-B8000
[0.000] MAIN INFO : - framebuffer pitch  160
[0.000] MAIN INFO : - framebuffer width  80
[0.000] MAIN INFO : - framebuffer height 25
[0.000] MAIN INFO : - framebuffer bpp    16
[0.000] MAIN INFO : - framebuffer type   2
[0.000] MAIN INFO : Initializing Global Descriptor Table...
[0.000] MAIN INFO : Initializing Interrupts Descriptor Table...
[0.000] MAIN INFO : Initializing Programmable Interrupt Controller...
[0.000] MAIN INFO : Initializing Programmable Interval Timer...
[0.000] MAIN INFO : Initializing Real Time Clock...
[0.000] MAIN INFO : Initializing Physical Memory Manager...
[0.000] MAIN INFO : Initializing Serial Port 1 for logging...
[0.000] MAIN INFO : Switching logging to serial port
[0.000] MAIN INFO : Initializing Kernel Heap...
[0.000] MAIN INFO : Initializing virtual memory mapping...
[0.000] MAIN INFO : Enabling interrupts & NMI...
[0.002] MAIN INFO : Detecting PCI devices...
[0.007] ATA INFO : Detected primary master ATA drive: "QEMU HARDDISK"
[0.014] MAIN INFO : Creating RAM disk...
[0.020] MAIN INFO : Initializing file system...
[0.022] PART INFO : Disks found:
[0.023] PART INFO : - dev #1: IDE primary master (drive_no #0): QEMU HARDDISK
[0.024] PART INFO : - dev #2: RAM disk (4096 KB at 0x54B000)
[0.025] PART INFO : Partitions found:
[0.026] PART INFO : - dev #1, p #1: Primary partition 1 (18432 sectors, start at 2048)
[0.026] MOUNT DEBUG: Mounting dev 1, part 1 as root. Pass "root=dnpn" to kernel to change
[0.026] MOUNT TRACE: vfs_mount(1, 1, "/")
[0.031] MOUNT INFO : Device 1 partition 1, driver "FAT", now mounted on "/"
[0.031] MOUNT DEBUG: Trying to mount remaining partitions
[0.032] MAIN INFO : Initializing multi-tasking...
[0.034] MAIN INFO : Running tests...
[0.041] UNITTEST INFO : 5 tests executed, 5 passed, 0 failed
[0.043] MAIN INFO : Giving the console to TTY manager...
[0.046] MAIN INFO : Starting multitasking, goodbye from main()!
[0.077] MAIN INFO : Shell launcher started, PID 2
[0.077] EXEC INFO : execve(): starting process /bin/sh[8]
[0.077] MAIN INFO : Shell launcher started, PID 3
[0.077] EXEC INFO : execve(): starting process /bin/sh[9]
[0.077] MAIN INFO : Shell launcher started, PID 4
[0.077] EXEC INFO : execve(): starting process /bin/sh[10]
[0.077] MAIN INFO : Shell launcher started, PID 5
[0.077] EXEC INFO : execve(): starting process /bin/sh[11]
```