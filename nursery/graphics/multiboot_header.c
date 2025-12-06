#include "multiboot2.h"

/* Multiboot2 header, in its own section */
__attribute__((section(".multiboot"), used))
const struct {
    multiboot_uint32_t magic;
    multiboot_uint32_t architecture;
    multiboot_uint32_t header_length;
    multiboot_uint32_t checksum;
    struct {
        multiboot_uint16_t type;
        multiboot_uint16_t flags;
        multiboot_uint32_t size;
    } end_tag;
} mb2_header = {
    .magic = MULTIBOOT2_HEADER_MAGIC,
    .architecture = 0,
    .header_length = sizeof(mb2_header),
    .checksum = -(MULTIBOOT2_HEADER_MAGIC + 0 + sizeof(mb2_header)),
    .end_tag = {
        .type = 0, // MULTIBOOT_HEADER_TAG_END
        .flags = 0,
        .size = sizeof(mb2_header.end_tag)
    }
};
