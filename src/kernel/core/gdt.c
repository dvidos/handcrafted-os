#include <stdint.h>
#include <stddef.h>
#include "string.h"
#include "klog.h"

// Each define here is for a specific flag in the descriptor.
// Refer to the intel documentation for a description of what each one does.
// also https://wiki.osdev.org/GDT



// access byte
#define ACCESS_PRESENT(x)          (((x) & 0x01) << 7) // Present. 1 for all valid segments
#define ACCESS_PRIVILEGE(x)        (((x) & 0x03) << 5) // Set privilege level (0 highest - 3 lowest)
#define ACCESS_DESCRIPTOR_TYPE(x)  (((x) & 0x01) << 4) // Descriptor type (0 for system, 1 for code or data)
#define ACCESS_EXECUTABLE(x)       (((x) & 0x01) << 3) // 1 for code, 0 for data
#define ACCESS_DATA_GROW_DOWN(x)   (((x) & 0x01) << 2) // for data segments: 0 grow up, 1 grow down
#define ACCESS_CODE_CONFORMING(x)  (((x) & 0x01) << 2) // for code segments: 0 exec only from same privilege level, 1 exec from same or lower level
#define ACCESS_DATA_WRITABLE(x)    (((x) & 0x01) << 1) // for data segments, allow write or not
#define ACCESS_CODE_READABLE(x)    (((x) & 0x01) << 1) // for code segments, allow read or not
#define ACCESS_ACCESSED(x)         ((x) & 0x01)        // best left clear, CPU will set to 1 when accessed

// flags
#define FLAGS_GRANULARITY(x)       (((x) & 0x01) << 3) // 0 for limit in bytes, 1 for limit in 4 kb blocks
#define FLAGS_SIZE(x)              (((x) & 0x01) << 2) // 0 for 16 bit protected mode segment, 1 for 32 bit protected mode segment
#define FLAGS_LONG(x)              (((x) & 0x01) << 1) // 0 for 16/32 bit, 1 for long mode (64 bit)

struct gdt_segment_descriptor32 {
    uint16_t limit_low16;
    uint16_t base_low16;
    uint8_t  base_3rd_byte;
    uint8_t  access;
    uint8_t  limit_high_4bits: 4;
    uint8_t  flags: 4;
    uint8_t  base_4th_byte: 1;
} __attribute__((packed));

struct gdt_descriptor32 {
    uint16_t size;
    uint32_t offset;
} __attribute__((packed));

struct gdt_descriptor64 {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed));

struct gdt_segment_descriptor32 descriptors[5];
struct gdt_descriptor32 gdt;


// this method defined in assembly
extern void load_gdt_descriptor(uint32_t);




// +----------+----------------------+----------+----------+----------+----------+----------+----------+
// | byte 7   |        byte 6        | byte 5   | byte 4   | byte 3   | byte 2   | byte 1   | byte 0   |
// +----------+----------+-----------+----------+----------+----------+----------+----------+----------+
// | 63 .. 56 | 55 .. 52 | 51 .. 48  | 47 .. 40 | 39 .. 32 | 31 .. 24 | 23 .. 16 | 15 .. 8  | 7 .. 0   |
// | base     | flags    | limit     | access   | base     | base     | base     | limit    | limit    |
// | 4th byte | 4 bits   | hi 4 bits | 8 bits   | 3rd byte | 2nd byte | low byte | 2nd byte | low byte |
// +----------+----------+-----------+----------+----------+----------+----------+----------+----------+
// see also https://github.com/programmingmind/OS/blob/master/tables.c
void set_descriptor(uint8_t entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    if (limit > 65536) {
        limit = limit >> 12; // now measuring in 4kb chunks instead of bytes
        flags |= FLAGS_GRANULARITY(1);
    }

    descriptors[entry].limit_low16 = limit & 0xFFFF;
    descriptors[entry].base_low16 = base & 0xFFFF;
    descriptors[entry].base_3rd_byte = (base >> 16) & 0xFF;
    descriptors[entry].access = access;
    descriptors[entry].limit_high_4bits = (limit >> 16) & 0x0F;
    descriptors[entry].flags = flags;
    descriptors[entry].base_4th_byte = (base >> 24) & 0xFF;
}


// prepares and loads the Global Descriptor Table
// this is needed for segmenting in protected mode
void init_gdt() {

    memset((char *)descriptors, 0, sizeof(descriptors));

    // first entry always zero
    set_descriptor(0, 0, 0, ACCESS_PRESENT(0), 0);

    // segment 0x8 (decimal 8) will be our code segment (e.g. "jmp 0x8:some_func")
    set_descriptor(1,
        0,
        0xffffffff,  // 4 GB
        ACCESS_PRESENT(1) | ACCESS_DESCRIPTOR_TYPE(1) | ACCESS_EXECUTABLE(1) | 
        ACCESS_CODE_READABLE(1) | ACCESS_PRIVILEGE(0),
        FLAGS_SIZE(1));

    // segment 0x10 (decimal 16) will be our data segment (e.g. "mov eax 0x10:some_label")
    set_descriptor(2,
        0,
        0xffffffff,  // 4 GB
        ACCESS_PRESENT(1) | ACCESS_DESCRIPTOR_TYPE(1) | ACCESS_EXECUTABLE(0) | 
        ACCESS_DATA_WRITABLE(1) | ACCESS_PRIVILEGE(0),
        FLAGS_SIZE(1));

    // segment 0x18 (decimal 24) will be our user land code segment (e.g. "jmp 0x18:some_func")
    set_descriptor(3,
        0,
        0xffffffff,  // 4 GB
        ACCESS_PRESENT(1) | ACCESS_DESCRIPTOR_TYPE(1) | ACCESS_EXECUTABLE(1) | 
        ACCESS_CODE_READABLE(1) | ACCESS_PRIVILEGE(3),
        FLAGS_SIZE(1));

    // segment 0x20 (decimal 32) will be our user land data segment (e.g. "mov eax 0x20:some_label")
    set_descriptor(4,
        0,
        0xffffffff,  // 4 GB
        ACCESS_PRESENT(1) | ACCESS_DESCRIPTOR_TYPE(1) | ACCESS_EXECUTABLE(0) | 
        ACCESS_DATA_WRITABLE(1) | ACCESS_PRIVILEGE(3),
        FLAGS_SIZE(1));

    klog_debug("Size of GDT segment descriptor: %d", sizeof(struct gdt_segment_descriptor32));  // 8
    klog_debug("Size of all descriptors: %d", sizeof(descriptors));                             // 24
    klog_debug("Size of GDT descriptor: %d", sizeof(struct gdt_descriptor32));                  // 6

    gdt.size = sizeof(descriptors);
    gdt.offset = (uint32_t)descriptors;
    load_gdt_descriptor((uint32_t)&gdt);
}
