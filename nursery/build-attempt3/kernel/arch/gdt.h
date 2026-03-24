#ifndef _GDT_H
#define _GDT_H


#define KERNEL_CODE_SEGMENT    0x08  //  8, ring 0  executable, read-only
#define KERNEL_DATA_SEGMENT    0x10  // 16, ring 0, read-write
#define USER_CODE_SEGMENT      0x18  // 24, ring 3, executable, read-only
#define USER_DATA_SEGMENT      0x20  // 32, ring 3, read-write
#define TSS_SELECTOR           0x28

extern uintptr_t tss_address; // for use in assembly

void init_gdt();


#endif
