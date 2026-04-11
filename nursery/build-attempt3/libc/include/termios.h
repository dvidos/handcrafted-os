#ifndef _TERMIOS_H
#define _TERMIOS_H

#include "kernel/ioctl.h"


typedef unsigned int  tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int  speed_t;

#define NCCS 32 // Plenty of room for standard and custom control keys



struct termios {
    tcflag_t c_iflag;      // Input modes
    tcflag_t c_oflag;      // Output modes
    tcflag_t c_cflag;      // Control modes
    tcflag_t c_lflag;      // Local modes
    cc_t     c_cc[NCCS];   // Special control characters
    speed_t  c_ispeed;     // Input speed (baud)
    speed_t  c_ospeed;     // Output speed (baud)
};







#endif // _TERMIOS_H