# uap (user api)

This folder to contain headers exported externally.
Typically libc will use them and expose them to user apps.

They are the stable API of the kernel.

Examples are:

* structures for stat, time, dirent, 
* signal numbers, 
* syscall numbers,
* error codes, 
* file mode bits, open flags, seek flags,
* keyboard scan codes etc

