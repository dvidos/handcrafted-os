#include <ctypes.h>
#include <va_list.h>

// my original approach was different:
// - have a constructor function allocate and return a pointer to the object
// - the object will have an "ops" member with pointers to functions.
// - this might not support inheritence or polymorphism, but it's clean enough.

// based on this pdf: https://www.cs.rit.edu/~ats/books/ooc.pdf
// immitation of c++ classes implementation, before getting to the ugly side of syntax.

// requirements for this system:
// - polymorphism (e.g. substitute with child classes without client knowing)
// - inheritance (e.g. easily define new child classes, with a few different methods)
// - ease of use (e.g. intellisense)
// - avoiding polluting the namespace (e.g. not too many non-static methods)
// I think in my approach I am doing "strongly typed" approach (in the C sense)
// while the book uses a "void *" approach.



// ---------------------------------------------------------------------------



// base class creation system
// - offers new() and delete()
// - to create a new object, pass a pointer to a class_info instance.
// - in the future: clone(), compare(), equals(), hash(), to_string(), size_of(), is_a() etc
// - the first member of the object instance will be a pointer to this class_info instance.

struct class_info {
    size_t size;
    void *(*costructor)(void *this, va_list *args);
    int (*destructor)(void *this);
};

void *new(struct class_info *class_info, ...) {
    void *instance = malloc(class_info->size);
    assert(instance);

    // assign class_info (this pointer must be the first member of the object)
    *(struct class_info **)instance = class_info;

    if (class_info->costructor) {
        va_list args;
        va_start(args, class_info);
        void *obj = class_info->costructor(instance, args);
        va_end(args);
    }

    return instance;
}

void delete(void *instance) {
    if (instance == NULL)
        return;

    struct class_info *class_info = *(struct class_info **)instance;
    if (class_info && class_info->destructor) {
        class_info->destructor(instance);
    }

    free(instance);
}


// ---------------------------------------------------------------------


// creation of a class
// 1. create the instance structure
// 2. write constructor and destructor, if applicables
// 3. initialize one class_info instance with size, ctor and dtor
// 4. define a pointer to the class_info instance.

struct storage_device {
    struct class_info *class;

    // instance properties
    int sector_size;
    int (*read)(struct storage_device *this, unsigned int sector_num, char *buffer);
    int (*write)(struct storage_device *this, unsigned int sector_num, char *buffer);
};

static int storage_device_read(struct storage_device *this, unsigned int sector_num, char *buffer) {
    return -1;
}

static int storage_device_write(struct storage_device *this, unsigned int sector_num, char *buffer) {
    return -1;
}

static void storage_device_constructor(void *instance, va_list args) {
    struct storage_device *this = (struct storage_device *)instance;
    this->sector_size = 512;
    this->read = storage_device_read;
    this->write = storage_device_write;
}

static struct class_info _storage_device_class_info = {
    .size = sizeof(struct storage_device),
    .costructor = storage_device_constructor,
};

struct class_info *StorageDevice = &_storage_device_class_info;

void demo1() {
    char buffer[512];
    struct storage_device *dev = new(StorageDevice);

    if (dev->read(dev, 12, buffer) == 0)
        dev->write(dev, 24, buffer);
    delete(dev);
}




// --------------------------------------------------------------------

// attempting to subclass something
// don't care too much for inheritance, but for polymorphism.
// - if we want binary replacement of base class, we MUST have the same pointers in the same place.
//   that means, changing the parent we have to change all children and grandchildren too.
// - do we change the size of the allocated memory?
// - can this be strongly typed?
// - maybe all common children must point to the common parent.

struct usb_storage_device___bad_design___do_not_use {
    struct class_info *class;
    char *device_name; // <-- this extra member blows up binary replacement....
    int sector_size;
    int (*read)(struct storage_device *this, unsigned int sector_num, char *buffer);
    int (*write)(struct storage_device *this, unsigned int sector_num, char *buffer);
    int (*eject)(struct usb_storage_device2 *this);
};

struct usb_storage_device {
    // having the parent data here in a binary compatible way,
    // means we can pass this structure to any structure that expects a parent class.
    // properties and method pointers will be correct.
    struct storage_device super;
    char *device_name; // <-- this is now binary appended
    int (*eject)(struct usb_storage_device *this);
};

static int usb_storage_device_read(struct usb_storage_device *this, unsigned int sector_num, char *buffer) {
    return -1;
}

static int usb_storage_device_write(struct usb_storage_device *this, unsigned int sector_num, char *buffer) {
    return -1;
}

static int usb_storage_device_eject(struct usb_storage_device *this) {
    return -1;
}

static void usb_storage_device_constructor(void *instance, va_list args) {
    struct usb_storage_device *this = (struct usb_storage_device *)instance;

    this->device_name = va_arg(args, char *);
    this->super.sector_size = 512;
    this->super.read = usb_storage_device_read;  // overriding the base method
    this->super.write = usb_storage_device_write; // overriding the base method
    this->eject = usb_storage_device_eject; // new method.

}

static struct class_info _usb_storage_device_class_info = {
    .size = sizeof(struct usb_storage_device),
    .costructor = usb_storage_device_constructor,
};

struct class_info *UsbStorageDevice = &_usb_storage_device_class_info;

void demo2() {
    char buffer[512];
    struct storage_device *dev1 = new(StorageDevice);
    struct storage_device *dev2 = new(UsbStorageDevice, "/dev/usb1");
    struct usb_storage_device *usb_dev = (struct usb_storage_device *)dev2;

    if (dev1->read(dev1, 12, buffer) == 0)
        dev2->write(dev2, 24, buffer);

    // very ugly syntax...
    usb_dev->super.read(usb_dev, 32, buffer);

    // sure, we can use extra methods
    usb_dev->eject(usb_dev);
    
    delete(dev1);
    delete(dev2);
}


// -----------------------------------------------------------------

// here's another idea.

// it seems we have to have a struct with methods, and all object instances will point to them.
// when calling the methods, we pass the specific instances, that contain the specific data.
// this allows strong typing, but also polymorphism, as different constructors/factories
// can initialize to a different set of method pointers. 
// the "methods" structure is like the public signature, or the base class.
// such a system has no inheritence, but has polymorphism.
// - but how does this tie into a central `new()` and `delete()` ?
// to define alternate child classes, we need different constructor pointers.








// -------- base and child classes -------------------------------

// i think for each "class_info" in OOP, we need:
// - one structure for the public methods (e.g. signatures & pointers)
// - one structure for the instance of the object (e.g. variables & method struct)
// - one structure for the class_info of the object (e.g. methods)
// - what if we want to extend the base methods?

struct storage_device_public_methods {
    int (*sector_size)();
    int (*read)(int sector_no, char *buffer);
    int (*write)(int sector_no, char *buffer);
};

struct storage_device_public_methods storage_device_public_methods = {
    .sector_size = NULL,
    .read = NULL,
    .write = NULL;
};

struct storage_device_class {
    struct class_info *parent_class;
    // how is the constructor setting these up?
    struct storage_device_public_methods *ops;
};

struct storage_device_obj {
    struct class_info *class_info;
    struct storage_device_public_methods *ops;
};

struct storage_device_class _storage_device_class = {
    .ops = &storage_device_public_methods,
};
struct storage_device_class *StorageDevice = &_storage_device_class;

void storage_device_constructor(void *obj, va_list args) {
    
}


#define PRIMARY    1
#define SECONDARY  2
#define MASTER     1
#define SLACE      2


struct ata_drive_class_info {
    struct class_info *parent_class; // storage_device

};

struct ata_drive _ata_drive = {

};
struct ata_drive *AtaDrive = &_ata_drive;

void ata_drive_constructor(void *obj, va_list args) {

}

int ata_drive_sector_size() {
    return 512;
}
int ata_drive_read(int sector_no, char *buffer) {
    // use inb() / outb() to perform operation
}
int ata_drive_write(int sector_no, char *buffer) {
    // use inb() / outb() to perform operation
}

struct sata_drive {

};

void sata_drive_constructor(void *obj, va_list args) {

}
int sata_drive_sector_size() {
    return 512;
}
int sata_drive_read(int sector_no, char *buffer) {
    // use PCI methods to perform operation
}
int sata_drive_write(int sector_no, char *buffer) {
    // use PCI methods to perform operation
}

struct usb_drive_public_methods {

};

struct usb_drive {
    // this should contain all public accessible data and methods
    struct storage_device_public_methods storage_dev_ops;
    
};
struct usb_drive _usb_drive {

};
struct usb_drive *UsbDrive = &_usb_drive;

int usb_drive_sector_size() {
    return 512;
}
int usb_drive_read(int sector_no, char *buffer) {
    // use USB layer to perform operation
}
int usb_drive_write(int sector_no, char *buffer) {
    // use USB layer to perform operation
}


// ------- specific class_info code ----------------------------------



// ------- demo code ----------------------------------

void demo() {
    // define an array of four storage devices
    // they should be constructed differently,
    // but they should have the same exact method signatures and binary interface

    struct usb_drive *d1 = new(UsbDrive, "/dev/usb1");
    struct ata_drive *d2 = new(AtaDrive, PRIMARY, MASTER);

    char buffer[512];
    int err;

    err = d1->ops->read(17, buffer);
    if (!err)
        d1->ops->write(64, buffer);

    delete(d1);
    delete(d2);

}

