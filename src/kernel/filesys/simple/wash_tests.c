#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "simple_filesystem.h"
#include "dependencies/returns.h"


#define MAX_FILEPATH_LEN      256
#define MAX_DATA_SIZE         (10*1024)
#define MAX_TRACKED_FILES     256

typedef struct test_file test_file;
typedef struct test_scenario test_scenario;
typedef enum test_operation test_operation;

// represents the expected state of a file in memory
struct test_file {
    char      name[MAX_FILEPATH_LEN];
    uint8_t   *data;    // a buffer holding expected file content
    uint32_t  size;
    int       exists;
};

// this represents the whole scenario
struct test_scenario {
    test_file files[MAX_TRACKED_FILES];
    int       file_count;
    uint32_t  seed;
    simple_filesystem *fs;
};

enum test_operation {
    OP_CREATE = 0,
    OP_WRITE,
    OP_READ,
    OP_DELETE,
    OP_RENAME,
    OP_STAT,
    OP_TRUNC,
    OP_LISTDIR,
    NUM_OPERATIONS // actually counts size
};

// -----------------------------------------------

uint8_t random_byte(test_scenario *scenario) {
    return (uint8_t)(rand_r(&scenario->seed) & 0xff);
}
void random_name(test_scenario *scenario, char *buffer, int maxlen) {
    int len = rand_r(&scenario->seed) % maxlen;
    for (int i = 0; i < len; i++)
        buffer[i] = 'a' + (rand_r(&scenario->seed) % 26);
    buffer[len] = 0;
}
test_file *pick_random_file(test_scenario *scenario) {
    if (scenario->file_count == 0) return NULL;
    int tries = 10000;
    while (tries-- > 0) {
        int index = rand_r(&scenario->seed) % MAX_TRACKED_FILES;
        if (scenario->files[index].name[0] != 0)
            return &scenario->files[index];
    }
    return NULL; // we didn't find any
}
test_operation random_operation(test_scenario *scenario) {
    return (enum test_operation)(rand_r(&scenario->seed) % NUM_OPERATIONS);
}


// -----------------------------------------------

static void test_create(test_scenario *ts) {
    int err;

    if (ts->file_count >= MAX_TRACKED_FILES)
        return;

    char new_name[50];
    new_name[0] = '/';
    random_name(ts, new_name + 1, sizeof(new_name) - 1);

    // action-under-test
    err = ts->fs->create(ts->fs, new_name, 0);
    assert(err == OK);

    // ensure it exists
    sfs_stat_info info;
    err = ts->fs->stat(ts->fs, new_name, &info);
    assert(err == OK);

    // update our view
    for (int i = 0; i < MAX_TRACKED_FILES; i++) {
        if (ts->files[i].name[0] != 0)
            continue;

        strncpy(ts->files[i].name, new_name, sizeof(ts->files[i].name));
        ts->files[i].data = malloc(1);
        ts->files[i].exists = 1;
        ts->files[i].size = 0;
        ts->file_count++;
        break;
    }
}


static void test_write(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    if (!f->exists) return;

    uint32_t offset = (rand_r(&ts->seed) % (f->size + 1));
    uint32_t len    = 1 + rand_r(&ts->seed) % 100;

    char *buffer = malloc(len);
    for (int i = 0; i < len; i++)
        buffer[i] = (char)(rand_r(&ts->seed) & 0xFF);

    // perform operation
    sfs_handle *h = NULL;
    int err;
    err = ts->fs->open(ts->fs, f->name, 0, &h);
    assert(err == OK);
    assert(h != NULL);

    err = ts->fs->seek(ts->fs, h, offset, 0);
    assert(err == OK);

    err = ts->fs->write(ts->fs, h, buffer, len);
    assert(err == len);

    // update our view
    if (offset + len > f->size) {
        f->data = realloc(f->data, offset + len);
        f->size = offset + len;
    }
    memcpy(f->data + offset, buffer, len);

    // verify
    err = ts->fs->seek(ts->fs, h, offset, 0);
    assert(err == OK);

    char *check = malloc(len);
    err = ts->fs->read(ts->fs, h, check, len);
    assert(err == len);

    assert(memcmp(check, buffer, len) == 0);

    err = ts->fs->close(ts->fs, h);
    assert(err == OK);

    free(buffer);
    free(check);
}

static void test_read_verify(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    if (!f->exists) return;

    uint32_t len    = 1 + rand_r(&ts->seed) % 100;
    uint32_t offset = (rand_r(&ts->seed) % (f->size - len));

    char *buffer = malloc(len);
    for (int i = 0; i < len; i++)
        buffer[i] = (char)(rand_r(&ts->seed) & 0xFF);

    // perform operation
    sfs_handle *h = NULL;
    int err;
    err = ts->fs->open(ts->fs, f->name, 0, &h);
    assert(err == OK);
    assert(h != NULL);

    err = ts->fs->seek(ts->fs, h, offset, 0);
    assert(err == OK);

    err = ts->fs->read(ts->fs, h, buffer, len);
    assert(err == len);

    err = ts->fs->close(ts->fs, h);
    assert(err == OK);

    assert(memcmp(buffer, f->data + offset, len) == 0);

    free(buffer);
}

static void test_delete(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    if (!f->exists) return;
    sfs_stat_info info;
    int err;

    // verify behavior
    err = ts->fs->unlink(ts->fs, f->name, 0);
    assert(err == OK);

    err = ts->fs->stat(ts->fs, f->name, &info);
    assert(err == ERR_NOT_FOUND);

    // update our view
    f->exists = 0;
}

static void test_rename(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    if (!f->exists) return;

    char new_name[50];
    new_name[0] = '/';
    random_name(ts, new_name + 1, sizeof(new_name) - 1);

    int err = ts->fs->rename(ts->fs, f->name, new_name);
    assert(err == OK);

    // udpate our view
    strncpy(f->name, new_name, sizeof(f->name));
}

static void test_stat_verify(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    sfs_stat_info info;
    int err;

    if (f->exists) {
        err = ts->fs->stat(ts->fs, f->name, &info);
        assert(err == OK);
        assert(info.file_size == f->size);
    } else {
        err = ts->fs->stat(ts->fs, f->name, &info);
        assert(err == ERR_NOT_FOUND);
    }
}

static void test_truncate(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    if (!f->exists) return;
    if (f->size == 0) return;

    int err = ts->fs->truncate(ts->fs, f->name);
    assert(err == OK);

    // verify
    sfs_stat_info info;
    err = ts->fs->stat(ts->fs, f->name, &info);
    assert(err == OK);
    assert(info.file_size == 0);

    // update our view
    f->size = 0;
}

static void test_list_verify(test_scenario *ts) {
}

// ------------------------------------------------------------

static void verify_all_files(test_scenario *ts) {
    sfs_stat_info info;
    int err;

    for (int i = 0; i < MAX_TRACKED_FILES; i++) {
        test_file *f = &ts->files[i];
        if (f->name[0] == 0) continue;

        if (f->exists) {
            // verify it exists, check length, contents
            err = ts->fs->stat(ts->fs, f->name, &info);
            assert(err == OK);
            assert(info.file_size == f->size);

        } else {
            // verify it does not exist
            err = ts->fs->stat(ts->fs, f->name, &info);
            assert(err == ERR_NOT_FOUND);
        }
    }
}

// ------------------------------------------------------------

// this runs the scenario
void run_scenario(unsigned int seed, int steps) {
    printf("Running scenario %d, %d steps\n", seed, steps);
    mem_allocator *mem = new_malloc_based_mem_allocator();
    sector_device *dev = new_mem_based_sector_device(512, 65536); // 32 MB disk
    clock_device *clk = new_fixed_clock_device(123);
    simple_filesystem *fs = new_simple_filesystem(mem, dev, clk);

    test_scenario scenario = { .seed = seed, .file_count = 0, .fs = fs };
    test_scenario *ts = &scenario;

    for (int i = 0; i < steps; i++) {
        test_operation op = random_operation(ts);
        switch (op) {
            case OP_CREATE:   test_create(ts);      break;
            case OP_WRITE:    test_write(ts);       break;
            case OP_READ:     test_read_verify(ts); break;
            case OP_DELETE:   test_delete(ts);      break;
            case OP_RENAME:   test_rename(ts);      break;
            case OP_STAT:     test_stat_verify(ts); break;
            case OP_TRUNC:    test_truncate(ts);    break;
            case OP_LISTDIR:  test_list_verify(ts); break;
            default: break; // do nothing
        }
    }

    verify_all_files(ts);

    // we should really release memory here...
}

int main() {
    // we'll run random numbers, later in a loop
    run_scenario(1234567, 100);
    return 0;
}

// ------------------------------------------------------------