#include "dependencies/returns.h"
#include "dependencies/mem_allocator.h"
#include "dependencies/sector_device.h"
#include "simple_filesystem.h"
#include <assert.h>
#include <stdio.h>


static void mkfs_test() {
    int err;

    mem_allocator *m = new_malloc_based_mem_allocator();
    sector_device *d = new_mem_based_sector_device(512, 4096);

    // we need device and memory allocator
    simple_filesystem *fs = new_simple_filesystem(m, d);


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

static void wash_test() {
    // make 500 files, randomly adding blocks to each, till something breaks or file is full?
}

void run_tests() {
    mkfs_test();
    wash_test();

    // opendir,
    // readdir
    // closedir

    // create file
    // write
    // close 

    // delete

    // etc
}


int main() {
    printf("Running tests... ");
    run_tests();
    printf("Done\n");
    return 0;
}
