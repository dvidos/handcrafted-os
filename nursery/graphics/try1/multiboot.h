#ifndef MULTIBOOT_HEADER
#define MULTIBOOT_HEADER 1

#include <stdint.h>

/* Multiboot2 magic for GRUB */
#define MULTIBOOT2_HEADER_MAGIC 0xE85250D6

#define MULTIBOOT_HEADER_TAG_END         0
#define MULTIBOOT_HEADER_TAG_FRAMEBUFFER 5

struct multiboot_header_tag_end {
    uint16_t type;
    uint16_t flags;
    uint32_t size;
};

struct multiboot_header_tag_framebuffer {
    uint16_t type;
    uint16_t flags;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
};

struct multiboot2_header {
    uint32_t magic;
    uint32_t architecture;
    uint32_t header_length;
    uint32_t checksum;

    struct multiboot_header_tag_framebuffer fb_tag;
    struct multiboot_header_tag_end end_tag;
};

/* Multiboot info tag structures (read by kernel) */

struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    uint8_t  framebuffer_red_field_pos;
    uint8_t  framebuffer_red_mask_size;
    uint8_t  framebuffer_green_field_pos;
    uint8_t  framebuffer_green_mask_size;
    uint8_t  framebuffer_blue_field_pos;
    uint8_t  framebuffer_blue_mask_size;
};

#endif
