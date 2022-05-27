

// Linux Device Drivers 3rd edition ebook https://lwn.net/Kernel/LDD3/

// i think in this os, instead of treating everything as a file,
// we'll treat everything as a device.
// files, folders and shell will exist as well.
// a device can be a pipe in unix...
// or it can be a loopback device, or an echo device

// some devices notes:
// major number = device type (seen by "ls -l /dev", check two numbers by comma)
// minor number = device number for this family of devices (e.g. third USB port)
// unsupported operations can be left NULL
// most functions return a value, will return negarive values on error (e.g. EINVAL)
// reading and writing may need to block, something the driver should do
// device drivers usually maintain read and write buffers and block appropriately 
// when there is not enough space in the buffer to accept the user request
// it seems that the device driver calls the various process managing functions
// (e.g. yield(), block(), etc) directly.

struct device_operations {
    int (*open)(void *device);
    int (*close)(void *device);
    int (*read)(void *device, char *buffer, int size);
    int (*write)(void *device, char *buffer, int size);
    int (*seek)(void *device, int offset, int type);
    int (*ioctl)(void *device, int type, void *args); // args can be a number too
};

struct generic_device {
    
    void *driver_data;
    void *device_data;
    struct device_operations *ops;
};

typedef struct generic_device device_t;


// this makes it easy for several devices to be written:
// - /dev/null, /dev/random, /dev/zero
// - pipes
// - tty (screen & keyboard)
// - stdin, stdout, stderr, log
// - mice, sound devices, 
// - raw file partitions
// drives that can support many files are block devices in linux
// network adapters similarly, are special devices.
// it seems not everything can be mapped to read/write buffers... :-) 


// devices and or files will be stored in an array in process space
//

int open(char *path_or_device) {
    // so this opens a file or device
    return -1;
}

int write(int handle, char *buffer, int len) {
    // write to device or file
    return -1;
}

int read(int handle, char *buffer, int len) {
    return -1;
}

int seek(int handle, int offset, int origin) {
    return -1;
}

int ioctl(int handle, int type, char *args) {
    return -1;
}

int close(int handle) {
    return -1;
}

