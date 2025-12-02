#include "dependencies/returns.h"
#include "dependencies/mem_allocator.h"
#include "dependencies/sector_device.h"
#include "dependencies/clock_device.h"
#include "simple_filesystem.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define FS() \
    mem_allocator *mem = new_malloc_based_mem_allocator(); \
    sector_device *dev = new_mem_based_sector_device(512, 2048); \
    clock_device *clock = new_fixed_clock_device(123456); \
    simple_filesystem *fs = new_simple_filesystem(mem, dev, clock)

#define MOUNTED_FS()  \
    FS(); \
    assert(fs->mkfs(fs, "TEST", 0) == OK); \
    assert(fs->mount(fs, 0) == OK)



static void mkfs_test() {
    int err;
    FS();

    // make sure it fails to mount
    err = fs->mount(fs, 0);
    assert(err != OK);

    // make file system
    err = fs->mkfs(fs, "TEST", 0);
    assert(err == OK);

    // fs->dump_debug_info(fs, "After mkfs");

    err = fs->mount(fs, 0);
    assert(err == OK);

    // fs->dump_debug_info(fs, "After mount");

    err = fs->unmount(fs);
    assert(err == OK);

    // fs->dump_debug_info(fs, "After Unmount");

}

static void root_dir_test() {
    int err;
    MOUNTED_FS();

    sfs_handle *h;
    err = fs->open_dir(fs, "/", &h);
    assert(err == OK);

    sfs_dir_entry entry;
    while ((err = fs->read_dir(fs, h, &entry)) == OK) {
        printf("  %s\n", entry.name);
    }

    err = fs->close_dir(fs, h);
    assert(err == OK);
}

static void file_creation_test() {
    int err;

    mem_allocator *mem = new_malloc_based_mem_allocator(); \
    sector_device *dev = new_mem_based_sector_device(512, 2048); \
    clock_device *clock = new_fixed_clock_device(123456);
    simple_filesystem *fs = new_simple_filesystem(mem, dev, clock);


    assert(fs->mkfs(fs, "TEST", 0) == OK);
    assert(fs->mount(fs, 0) == OK);

    assert(fs->create(fs, "/unit_test.c", 0) == OK);
    assert(fs->create(fs, "/bin", 1) == OK);
    assert(fs->create(fs, "/bin", 1) == ERR_ALREADY_EXISTS);  // second time should fail
    assert(fs->create(fs, "/bin/sh.c", 0) == OK);
    assert(fs->create(fs, "/bin/sh.c/file", 0) == ERR_WRONG_TYPE); // cannot create inside a file
    assert(fs->create(fs, "/something/sh.c", 0) == ERR_NOT_FOUND);  // path was not found

    // delete
    sfs_handle *h;
    assert(fs->create(fs, "/bin/temp", 0) == OK);
    assert(fs->open(fs, "/bin/temp", 0, &h) == OK);
    assert(fs->close(fs, h) == OK);
    assert(fs->unlink(fs, "/bin/temp", 0) == OK);
    assert(fs->open(fs, "/bin/temp", 0, &h) == ERR_NOT_FOUND);

    // rename / move (unlink + link essentially)
    assert(fs->create(fs, "/dir1", 1) == OK);
    assert(fs->create(fs, "/dir2", 1) == OK);
    assert(fs->create(fs, "/dir1/file1", 0) == OK);
    assert(fs->rename(fs, "/dir1/file1", "/dir2/file2") == OK);
    assert(fs->unlink(fs, "/dir1/file1", 0) == ERR_NOT_FOUND);
    assert(fs->unlink(fs, "/dir2/file2", 0) == OK);


    err = fs->sync(fs);
    assert(err == OK);
    // fs->dump_debug_info(fs, "After file + dir creation");
    // dev->dump_debug_info(dev, "After files created");
}

static void simple_file_test() {
    int err;
    FS();

    err = fs->mkfs(fs, "TEST", 0); 
    assert(err == OK);

    err = fs->mount(fs, 0);
    assert(err == OK);

    err = fs->create(fs, "/file.txt", 0);
    assert(err == OK);
    err = fs->create(fs, "/file2.txt", 0);
    assert(err == OK);

    sfs_handle *h, *h2, *h3;

    err = fs->open(fs, "/file.txt", 1, &h);
    assert(err == OK);
    err = fs->open(fs, "/file2.txt", 1, &h2);
    assert(err == OK);
    err = fs->open(fs, "/file.txt", 1, &h3);
    assert(err == OK);

    // fs->dump_debug_info(fs, "Before writing");
 
    err = fs->write(fs, h, "Hello world!\n", 13);
    assert(err == 13);

    // fs->dump_debug_info(fs, "After writing");

    err = fs->close(fs, h);
    assert(err == OK);

    // fs->dump_debug_info(fs, "After closing");


    err = fs->open(fs, "/file.txt", 1, &h);
    assert(err == OK);

    // fs->dump_debug_info(fs, "After file creation");

    char buffer[64];
    err = fs->read(fs, h, buffer, sizeof(buffer));
    assert(err == 13);
    assert(memcmp(buffer, "Hello world!\n", 13) == 0);

    err = fs->close(fs, h);
    assert(err == OK);

    err = fs->unmount(fs);
    assert(err == OK);

    // dev->dump_debug_info(dev, "After creating and reading a text file");
}

void run_tests() {
    mkfs_test();
    root_dir_test();
    file_creation_test();
    simple_file_test();
}


int main() {
    printf("Running tests... \n");
    run_tests();
    printf("Done\n");
    return 0;
}
