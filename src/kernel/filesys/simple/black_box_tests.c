#include "dependencies/returns.h"
#include "dependencies/mem_allocator.h"
#include "dependencies/sector_device.h"
#include "dependencies/clock_device.h"
#include "simple_filesystem.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

#define assert_ok(err)   if (err != OK) { printf("%s:%d: assertion failure: expected OK, got %d\n", __FILE__, __LINE__, err); exit(1); }

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
    assert_ok(err);

    // fs->dump_debug_info(fs, "After mkfs");

    err = fs->mount(fs, 0);
    assert_ok(err);

    // fs->dump_debug_info(fs, "After mount");

    err = fs->unmount(fs);
    assert_ok(err);

    // fs->dump_debug_info(fs, "After Unmount");

}

static void root_dir_test() {
    int err;
    MOUNTED_FS();

    sfs_handle *h;
    err = fs->open_dir(fs, "/", &h);
    assert_ok(err);

    sfs_dir_entry entry;
    while ((err = fs->read_dir(fs, h, &entry)) == OK) {
        printf("  %s\n", entry.name);
    }

    err = fs->close_dir(fs, h);
    assert_ok(err);
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
    assert_ok(err);
    // fs->dump_debug_info(fs, "After file + dir creation");
    // dev->dump_debug_info(dev, "After files created");
}

static void simple_file_test() {
    int err;
    FS();

    err = fs->mkfs(fs, "TEST", 0); 
    assert_ok(err);

    err = fs->mount(fs, 0);
    assert_ok(err);

    err = fs->create(fs, "/file.txt", 0);
    assert_ok(err);
    err = fs->create(fs, "/file2.txt", 0);
    assert_ok(err);

    sfs_handle *h, *h2, *h3;

    err = fs->open(fs, "/file.txt", 1, &h);
    assert_ok(err);
    err = fs->open(fs, "/file2.txt", 1, &h2);
    assert_ok(err);
    err = fs->open(fs, "/file.txt", 1, &h3);
    assert_ok(err);

    // fs->dump_debug_info(fs, "Before writing");
 
    err = fs->write(fs, h, "Hello world!\n", 13);
    assert(err == 13);

    // fs->dump_debug_info(fs, "After writing");

    err = fs->close(fs, h);
    assert_ok(err);

    // fs->dump_debug_info(fs, "After closing");

    err = fs->open(fs, "/file.txt", 1, &h);
    assert_ok(err);

    char buffer[64];
    err = fs->read(fs, h, buffer, sizeof(buffer));
    assert(err == 13);
    assert(memcmp(buffer, "Hello world!\n", 13) == 0);

    err = fs->close(fs, h);
    assert_ok(err);

    err = fs->unmount(fs);
    assert_ok(err);
}

static void big_file_test(int buffsize, int times) {
    int err;
    FS();

    err = fs->mkfs(fs, "TEST", 0); 
    assert_ok(err);
    err = fs->mount(fs, 0);
    assert_ok(err);

    err = fs->create(fs, "/big-file.bin", 0);
    assert_ok(err);
    sfs_handle *h;
    err = fs->open(fs, "/big-file.bin", 1, &h);
    assert_ok(err);

    char *buffer = malloc(buffsize);
    for (int i = 0; i < buffsize; i++)
        buffer[i] = rand() & 0xFF;
    
    for (int i = 0; i < times; i++) {
        err = fs->write(fs, h, buffer, buffsize);
        assert(err == buffsize);
    }
    fs->dump_debug_info(fs, "After file creation");


    err = fs->close(fs, h);
    assert_ok(err);
    err = fs->unmount(fs);
    assert_ok(err);

    // dev->dump_debug_info(dev, "After big file");
}

struct test_file {
    char name[30];
    sfs_handle *handle;
};

static void many_files_test(int num_files, int write_size, int write_times) {
    int err;
    FS();

    err = fs->mkfs(fs, "TEST", 0); 
    assert_ok(err);
    err = fs->mount(fs, 0);
    assert_ok(err);


    struct test_file *files_arr = malloc(num_files * sizeof(struct test_file));
    for (int i = 0; i < num_files; i++) {
        sprintf(files_arr[i].name, "/file%04d.bin", i);

        err = fs->create(fs, files_arr[i].name, 0);
        assert_ok(err);

        err = fs->open(fs, files_arr[i].name, 0, &files_arr[i].handle);
        assert_ok(err);
    }

    char *buffer = malloc(write_size);
    for (int i = 0; i < write_size; i++)
        buffer[i] = rand() & 0xFF;
    
    for (int time = 0; time < write_times; time++) {
        // a round of writing to each file
        for (int i = 0; i < num_files; i++) {
            err = fs->write(fs, files_arr[i].handle, buffer, write_size);
            assert(err == write_size);
        }
    }

    fs->dump_debug_info(fs, "After many files creation");

    for (int i = 0; i < num_files; i++) {
        err = fs->close(fs, files_arr[i].handle);
        assert_ok(err);
    }
    err = fs->unmount(fs);
    assert_ok(err);

    // dev->dump_debug_info(dev, "After many files");
}

void run_tests() {
    // mkfs_test();
    // root_dir_test();
    // file_creation_test();
    // simple_file_test();
    // big_file_test(1022*1024, 1);
    many_files_test(3, 512, 50);
}


int main() {
    printf("Running tests... \n");
    run_tests();
    printf("Done\n");
    return 0;
}
