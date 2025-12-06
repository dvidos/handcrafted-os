#include "multiboot2.h"

/* Multiboot2 header, aligned and in its own section */
__attribute__((section(".multiboot"), used, aligned(8)))
const struct {
    multiboot_uint32_t magic;
    multiboot_uint32_t architecture;
    multiboot_uint32_t header_length;
    multiboot_uint32_t checksum;

    /* Framebuffer request tag */
    struct {
        multiboot_uint32_t type;
        multiboot_uint32_t size;
        multiboot_uint32_t width;
        multiboot_uint32_t height;
        multiboot_uint32_t bpp;
    } fb_request;

    /* End tag (required) */
    struct {
        multiboot_uint16_t type;
        multiboot_uint16_t flags;
        multiboot_uint32_t size;
    } end_tag;

} mb2_header = {
    .magic = MULTIBOOT2_HEADER_MAGIC,
    .architecture = 0, // 0 = i386
    .header_length = sizeof(mb2_header),
    .checksum = -(MULTIBOOT2_HEADER_MAGIC + 0 + sizeof(mb2_header)),

    // .fb_request = {
    //     .type = 0x20,             // MULTIBOOT_TAG_TYPE_FRAMEBUFFER_REQUEST
    //     .size = sizeof(((typeof(mb2_header)){0}).fb_request),
    //     .width = 1024,            // desired width
    //     .height = 768,            // desired height
    //     .bpp = 32                 // bits per pixel
    // },

    .end_tag = {
        .type = 0,                // MULTIBOOT_HEADER_TAG_END
        .flags = 0,
        .size = sizeof(((typeof(mb2_header)){0}).end_tag)
    }
};