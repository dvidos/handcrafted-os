# Polymorphism attempt

This third attempt has the following characteristics:

* Allows for polymorphism: different constructors are called, 
they set up their own private data and their own method pointers.
* Clean syntax for the client. `struct disk *d = new(IdeDisk, 1, 0);`
* If we assume that `object_info` pointer is the first member in the 
object structure, we can have an even cleaner `delete(ptr)` call.

It keeps a good balance between strongly typed pointers, 
and generic class manipulating methods (e.g. constructor, destructor, clone, equals(), hash() etc.)

The uniform `new()` and `delete()` functionality is purely cosmetical.
We could have specific methods for it.

## The public interface

The public interface must have the following elements:

* A structure containing:
  * a pointer to the `object_info` structure (as the first member)
  * any public data (can be modified by anyone)
  * a pointer to a structure with pointers to methods, typically called `ops`
  * a `void *` pointer to private data

* A structure containing pointers to public methods

The methods referenced in the methods structure will almost always
take a poiner to the structure as the first argument, typically named `self`.

Example:

```c
struct disk {
    struct object_info *object_info;

    struct disk_operations {
        int (*read)(struct disk *self, int sector, char *buffer);
        int (*write)(struct disk *self, int sector, char *buffer);
    } *ops;

    int sector_size;
    void *private_data;
};
```

## The discrete implementation

To implement a discrete implementation of the above interface we need the following:

* A structure for any private data of the implementation
* Functions for the public methods that implement the public interface
* A static variable of the function pointers structure, initialized accordingly
* A static constructor, that will initialize the object, 
and possibly allocate and initialize any private data as well
* A static variable of an `object_info` structure, initialized accordingly,
* A public pointer to the above variable, the only public symbol the module is exporting.

Example:

```c
struct ide_private_data {
    int controller_no;
    int slave_bit;
};

static int ide_disk_read(struct disk *self, int sector, char *buffer) {
    struct ide_private_data *pd = (struct ide_private_data *)self->private_data;
    return -1;
}

static int ide_disk_write(struct disk *self, int sector, char *buffer) {
    struct ide_private_data *pd = (struct ide_private_data *)self->private_data;
    return -1;
}

static struct disk_operations ide_operations = {
    .read = ide_disk_read,
    .write = ide_disk_write
};

static void ide_disk_constructor(void *instance, va_list args) {
    struct disk *disk = (struct disk *)instance;
    disk->sector_size = 512;
    disk->ops = &ide_operations;

    struct ide_private_data *pd = malloc(sizeof(struct ide_private_data));
    pd->controller_no = va_arg(args, int);
    pd->slave_bit = va_arg(args, int);
    disk->private_data = pd;
}

static void ide_disk_destructor(void *instance) {
    struct disk *disk = (struct disk *)instance;
    struct ide_private_data *pd = (struct ide_private_data *)disk->private_data;

    free(pd);
}

static struct object_info _ide_disk_info = {
    .size = sizeof(struct disk),
    .constructor = ide_disk_constructor,
    .destructor = ide_disk_destructor
};

struct object_info *IdeDisk = &_ide_disk_info;
```

As an example, the above interface also could be implemented for other storage media,
like a SATA disk, a RAM disk, or a USB stick, or mock objects for unit tests

## Using the interface and implementation

To use the polymorphic interface, one has to have the declaration
of the interface and the pointer to the `object_info` structure.

Example:

```c
#include "objects.h"
#include "disk.h"

void demo() {
    char buffer[512];

    struct disk *disk = new(IdeDisk, 0, 0);

    if (disk->ops->read(disk, 1, buffer))
        disk->ops->write(disk, 24, buffer);

    delete(disk);
}
```


## The "objects.h" subsystem

The current objects subsystem is a very simple `new(), delete()` pair, just for convenience.

We tried diving into implementing inheritence, but it becomes ugly fast, and we are losing
either type safety, or readable syntax. Maybe it's pointless to try to implement C++ using
C, since C++ already exists and is a choice.

For an object to be considered to be handled by this subsystem, the first member 
must be a pointer to an `object_info` structure. This contains meta information
on how the object is to be allocated or freed, for now.

```c
struct object_info {
    int size;
    void (*constructor)(void *instance, va_list args);
    void (*destructor)(void *instance);
};
```

Since this system supports different constructors for the various objects, it provides a clean 
syntax to create the objects, without resorting to discrete factory functions.


### Future extensions 

Fuuture operations can include: class_name(), clone(), compare(), size_of(), is_a(), equals(), hash(), 
to_string(), serialize(), unserialize(), etc.

We also have not investigated generating object_info dynamically, e.g. using a factory function.


## Inheritance & composition

It does not allow for inheritance, but I don't think we need it.
It does allow for composition, which is clean.

Private data can have pointers to other objects, in lower levels.

