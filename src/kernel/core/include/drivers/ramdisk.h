#ifndef _RAMDISK_H
#define _RAMDISK_H

#include <ctypes.h>

// creates a ram disk of some size and registers with device manager
void init_ramdisk(size_t size);


#endif
