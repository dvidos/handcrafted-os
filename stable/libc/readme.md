# libc

This is the library against which the user land programs shall be linked against.

The aims are:

* make it POSIX like, to make porting of various programs to our OS easier.
* add experimental features (e.g. containers, predicates, etc)
* add special features of our OS only (e.g. working with json objects and streams)

We shall need tests as well...


## kernel headers

One good way to get headers from kernel is to sanitize them in some way. E.g. given the following kernel header

```c
#ifndef _HCOS_ERRNO_H
#define _HCOS_ERRNO_H

/* UAPI_START */
#define ENOENT  2   /* No such file or directory */
#define EAGAIN  11  /* Try again */
#define ECHILD  10  /* No child processes */
/* UAPI_END */

// Internal kernel stuff - should be stripped
void kernel_log_error(int err); 

#endif
```

one can do the following:

```shell
sed -n '/UAPI_START/,/UAPI_END/p' kernel/include/shared/errno.h > rootfs/usr/include/bits/errno.h
```
