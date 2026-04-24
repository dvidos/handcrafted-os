#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#include "../kernel/ioctl.h"


int ioctl(int fd, unsigned long int request, unsigned long arg);




#endif /* _SYS_IOCTL_H */