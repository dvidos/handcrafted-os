#include <stdint.h>
#include <stddef.h>


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

#define BYTES_PER_DESCRIPTOR       8

// this method defined in assembly - expecting 2 args of 4 bytes each
extern void load_gdt_entries(void*, size_t);
// this method defined in assemble as well.
extern void reload_gdt_segments();


// this method to populate the table defined elsewhere
// +----------+----------------------+----------+----------+----------+----------+----------+----------+
// | byte 7   |        byte 6        | byte 5   | byte 4   | byte 3   | byte 2   | byte 1   | byte 0   |
// +----------+----------+-----------+----------+----------+----------+----------+----------+----------+
// | 63 .. 56 | 55 .. 52 | 51 .. 48  | 47 .. 40 | 39 .. 32 | 31 .. 24 | 23 .. 16 | 15 .. 8  | 7 .. 0   |
// | base     | flags    | limit     | access   | base     | base     | base     | limit    | limit    |
// | 4th byte | 4 bits   | hi 4 bits | 8 bits   | 3rd byte | 2nd byte | low byte | 2nd byte | low byte |
// +----------+----------+-----------+----------+----------+----------+----------+----------+----------+
// see also https://github.com/programmingmind/OS/blob/master/tables.c
void populate_segment_descriptor(uint8_t *ptr, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    if (limit > 65536) {
        limit = limit >> 12; // now measuring in 4kb chunks instead of bytes
        flags |= FLAGS_GRANULARITY(1);
    }

    ptr[0] = limit & 0xFF;
    ptr[1] = (limit >> 8) & 0xFF;
    ptr[2] = base & 0xFF;
    ptr[3] = (base >> 8) & 0xFF;
    ptr[4] = (base >> 16) & 0xFF;
    ptr[5] = access;
    ptr[6] = ((flags & 0x0F) << 4) | ((limit >> 16) & 0x0F);
    ptr[7] = (base >> 24) & 0xFF;
}

void init_gdt() {

    // each gdt entry needs 8 bytes
    // we'll create two segments for now
    // segment 0x8 (decimal 8) will be our code segment (e.g. "jmp 0x8:some_func")
    // segment 0x10 (decimal 16) will be our data segment (e.g. "mov eax 0x10:some_label")
    uint8_t gdt_buffer[3 * BYTES_PER_DESCRIPTOR];

    // first entry always zero
    populate_segment_descriptor(gdt_buffer + 0, 0, 0, ACCESS_PRESENT(0), 0);

    // our code entry
    populate_segment_descriptor(gdt_buffer + (BYTES_PER_DESCRIPTOR * 1),
        0,
        0xffffffff,  // 4 GB
        ACCESS_PRESENT(1) | ACCESS_DESCRIPTOR_TYPE(1) | ACCESS_EXECUTABLE(1) | ACCESS_CODE_READABLE(1),
        FLAGS_SIZE(1));

    // our data entry
    populate_segment_descriptor(gdt_buffer + (BYTES_PER_DESCRIPTOR * 2),
        0,
        0xffffffff,  // 4 GB
        ACCESS_PRESENT(1) | ACCESS_DESCRIPTOR_TYPE(1) | ACCESS_EXECUTABLE(0) | ACCESS_DATA_WRITABLE(1),
        FLAGS_SIZE(1));

    load_gdt_entries((void *)gdt_buffer, (size_t)sizeof(gdt_buffer));
    reload_gdt_segments();
}
