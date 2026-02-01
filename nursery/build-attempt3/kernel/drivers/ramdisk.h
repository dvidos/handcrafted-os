#ifndef _RAMDISK_H
#define _RAMDISK_H

#include <ctypes.h>

// creates a ram disk of some size and registers with device manager
void init_ramdisk(uint32_t start_address, uint32_t end_address);


#endif
