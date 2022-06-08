

## Kernel land

Possible kernel singletons:

* fs - file system
* mm - memory manager
* dm - device manager
* pm - process manager

Possible operations

* fs
    * mount / unmound drives
    * perform directory / file operations
    * provide statitics (free space etc)
* mm
    * allocate / deallocate single / multiple pages
    * allocate / deallocate areas from heap
    * provide statistics (free space etc)
* dm
    * provide device types list (storage, network, audio etc)
    * register device (type, number, operations)
    * get device operations
* pm
    * create / start / block / unblock process
    * exec
    * provide statistics (process list)


Examples of device types (each device type has discrete data & operations structures):

* storage device (ata, sata, usb, virtual etc)
* pseudo-tty (virtual)
* keyboard
* mouse
* screen
* clock / timer

## User land 

On user land, the same functionality 
will be provided via libc methods and syscall.
