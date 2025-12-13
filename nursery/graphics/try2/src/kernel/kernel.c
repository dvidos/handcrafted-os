#include <stdint.h>
#include "../boot_info.h"
#include "memory/string.h"
#include "graphics/graphics.h"
#include "graphics/color.h"

boot_info_t global_boot_info;



void kernel_main(boot_info_t* bi) {
    // preserve boot info
    memcpy(&global_boot_info, bi, sizeof(boot_info_t));
    graphics_initialize((char *)bi->fb.fb_addr, bi->fb.width, bi->fb.height, bi->fb.pitch, bi->fb.bpp);
    graphics_fill(0x008080);
    graphics_demo(100, 100, 255, 255);

    // kernel can never return, there's nothing to return to.
    for(;;) asm volatile("hlt");
}
