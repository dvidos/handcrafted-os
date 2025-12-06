#include "multiboot2.h"
#include <stdint.h>

void kmain(multiboot_info_t* mbi) {
    // Check for framebuffer availability
    if (!(mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        while(1); // halt if no framebuffer
    }

    uint8_t *fb = (uint8_t *) mbi->framebuffer_addr;
    uint32_t width  = mbi->framebuffer_width;
    uint32_t height = mbi->framebuffer_height;
    uint8_t bpp     = mbi->framebuffer_bpp;
    uint32_t pitch  = mbi->framebuffer_pitch;

    if (bpp != 32 || mbi->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
        while(1);

    // Draw a small green diagonal line
    for (uint32_t y = 0; y < width && y < height && y < 100; y++) {
        uint32_t offset = y * pitch + y * (bpp / 8);
        *((uint32_t *)(fb + offset)) = 0x00FF00; // green pixel
    }

    while(1);
}
