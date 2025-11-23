#include "dependencies/returns.h"
#include "dependencies/mem_allocator.h"
#include "dependencies/sector_device.h"
#include "simple_filesystem.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define FS()  \
    new_simple_filesystem( \
        new_malloc_based_mem_allocator(), \
        new_mem_based_sector_device(512, 4096) \
    );


static void mkfs_test() {
    int err;
    simple_filesystem *fs = FS()

    // make sure it fails to mount
    err = fs->mount(fs, 0);
    assert(err != OK);

    // make file system
    err = fs->mkfs(fs, "TEST", 0);
    assert(err == OK);
    // should also assert disk contents

    err = fs->mount(fs, 0);
    assert(err == OK);
    err = fs->unmount(fs);
    assert(err == OK);
}

static void simple_file_test() {
    int err;
    simple_filesystem *fs = FS()


    sfs_handle *h = fs->open(fs, "file.txt", 1);
    assert(h != NULL);

    err = fs->write(fs, h, "Hello world!\n", 13);
    assert(err == OK);

    err = fs->close(fs, h);
    assert(err == OK);


    h = fs->open(fs, "file.txt", 1);
    assert(h != NULL);

    char buffer[64];
    err = fs->read(fs, h, buffer, sizeof(buffer));
    assert(err == OK);
    assert(memcmp(buffer, "Hello world!\n", 13) == 0);

    err = fs->close(fs, h);
    assert(err == OK);
}

static void wash_test() {
    // make 500 files, randomly adding blocks to each, till something breaks or file is full?
}

void run_tests() {
    mkfs_test();
    wash_test();
    // simple_file_test();
    
}


int main() {
    printf("Running tests... ");
    run_tests();
    printf("Done\n");
    return 0;
}
