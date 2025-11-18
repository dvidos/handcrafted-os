#include "dependencies/returns.h"
#include "dependencies/mem_allocator.h"
#include "dependencies/sector_device.h"
#include "simplest_filesystem.h"
#include <assert.h>
#include <stdio.h>


void run_tests() {
    int err;

    mem_allocator *m = new_malloc_based_mem_allocator();
    sector_device *d = new_mem_based_sector_device(512, 4096);

    // we need device and memory allocator
    simplest_filesystem *fs = new_simplest_filesystem(m, d);


    // make sure it fails to mount
    err = fs->mount(fs, 0);
    assert(err != OK);

    // make file system
    err = fs->mkfs(fs, "TEST");
    assert(err == OK);
    // should also assert disk contents


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
