#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "simple_filesystem.h"
#include "dependencies/returns.h"

#define assert(x)  if (!(x)) { print_log(); printf("Assertion failed: %s\n", #x); exit(1); }
#define assert_no_err(err)   if (err != OK) { print_log(); printf("Returned status %d, at %s:%d\n", err, __FILE__, __LINE__); exit(1); }
#define assert_size(expected, actual)   if (expected != actual) { print_log(); printf("Expected size %d, but file has %d, at %s:%d\n", expected, actual, __FILE__, __LINE__); exit(1); }

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
    uint32_t  step_no; // for setting specific breakpoints
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

const char *test_operation_name(test_operation op) {
    switch (op) {
        case OP_CREATE: return "CREATE";
        case OP_WRITE: return "WRITE";
        case OP_READ: return "READ";
        case OP_DELETE: return "DELETE";
        case OP_RENAME: return "RENAME";
        case OP_STAT: return "STAT";
        case OP_TRUNC: return "TRUNC";
        case OP_LISTDIR: return "LISTDIR";
        default: return "(unknown operation)";
    }
}

// -----------------------------------------------

uint8_t random_byte(test_scenario *scenario) {
    return (uint8_t)(rand_r(&scenario->seed) & 0xff);
}
void random_name(test_scenario *scenario, char *buffer, int maxlen) {
    //int len = 3 + rand_r(&scenario->seed) % (maxlen - 3);
    int len = 3 + rand_r(&scenario->seed) % 10;
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
char *random_buffer(test_scenario *scenario, int len) {
    char *buffer = malloc(len);
    for (int i = 0; i < len; i++)
        buffer[i] = (char)(rand_r(&scenario->seed) & 0xFF);
    return buffer;
}

// -----------------------------------------------

typedef struct log_entry log_entry;
struct log_entry {
    uint32_t step_no;
    test_operation op;
    char *fname;
    char *fname2;
    uint32_t offset;
    uint32_t len;
};
log_entry *log_entries;
uint32_t log_entries_allocated;
uint32_t log_entries_count;

void initialize_log() {
    log_entries_allocated = 256;
    log_entries_count = 0;
    log_entries = malloc(log_entries_allocated * sizeof(log_entry));
}
void log_action(test_scenario *ts, test_operation op, char *fname, char *fname2, uint32_t offset, uint32_t len) {
    if (log_entries_count >= log_entries_allocated) {
        log_entries_allocated *= 2;
        log_entries = realloc(log_entries, log_entries_allocated * sizeof(log_entry));
    }
    log_entry *e = &log_entries[log_entries_count];
    e->op = op;
    e->fname = strdup(fname);
    e->fname2 = fname2 == NULL ? NULL : strdup(fname2);
    e->offset = offset;
    e->len = len;
    e->step_no = ts->step_no;
    log_entries_count++;
}
void print_log() {
    for (int i = 0; i < log_entries_count; i++) {
        log_entry *e = &log_entries[i];
        printf("step:%d:  %s '%s'", e->step_no, test_operation_name(e->op), e->fname);
        if (e->fname2 != NULL) printf(" '%s'", e->fname2);
        if (e->offset != -1) printf(" off=%d", e->offset);
        if (e->len != -1) printf(" len=%d", e->len);
        if (e->offset != -1 && e->len != -1)
            printf(" (off+len=%d)", e->offset + e->len);
        printf("\n");
    }
}

// -----------------------------------------------

static void test_create(test_scenario *ts) {
    int err;

    if (ts->file_count >= MAX_TRACKED_FILES)
        return;

    char new_name[50];
    new_name[0] = '/';
    random_name(ts, new_name + 1, sizeof(new_name) - 1);

    log_action(ts, OP_CREATE, new_name, NULL, -1, -1);

    // action-under-test
    err = ts->fs->create(ts->fs, new_name, 0);
    assert_no_err(err);

    // ensure it exists
    sfs_stat_info info;
    err = ts->fs->stat(ts->fs, new_name, &info);
    assert_no_err(err);

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
    uint32_t len    = 1 + rand_r(&ts->seed) % 1024;

    char *buffer = random_buffer(ts, len);

    log_action(ts, OP_WRITE, f->name, NULL, offset, len);

    // perform operation
    sfs_handle *h = NULL;
    int err;
    err = ts->fs->open(ts->fs, f->name, 0, &h);
    assert_no_err(err);
    assert(h != NULL);

    err = ts->fs->seek(ts->fs, h, offset, 0);
    assert_no_err(err);

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
    assert_no_err(err);

    char *check = malloc(len);
    err = ts->fs->read(ts->fs, h, check, len);
    assert(err == len);

    assert(memcmp(check, buffer, len) == 0);

    err = ts->fs->close(ts->fs, h);
    assert_no_err(err);

    free(buffer);
    free(check);
}

static void test_read_verify(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    if (!f->exists) return;
    if (f->size == 0) return;

    uint32_t len    = 1 + (rand_r(&ts->seed) % f->size);
    uint32_t offset = (rand_r(&ts->seed) % (f->size - len));
    char *buffer = malloc(len);

    log_action(ts, OP_READ, f->name, NULL, offset, len);

    // perform operation
    sfs_handle *h = NULL;
    int err;
    err = ts->fs->open(ts->fs, f->name, 0, &h);
    assert_no_err(err);
    assert(h != NULL);

    err = ts->fs->seek(ts->fs, h, offset, 0);
    assert_no_err(err);

    err = ts->fs->read(ts->fs, h, buffer, len);
    assert(err == len);

    err = ts->fs->close(ts->fs, h);
    assert_no_err(err);

    assert(memcmp(buffer, f->data + offset, len) == 0);

    free(buffer);
}

static void test_delete(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    if (!f->exists) return;
    sfs_stat_info info;
    int err;

    log_action(ts, OP_DELETE, f->name, NULL, -1, -1);

    // verify behavior
    err = ts->fs->unlink(ts->fs, f->name, 0);
    assert_no_err(err);

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

    log_action(ts, OP_RENAME, f->name, new_name, -1, -1);

    int err = ts->fs->rename(ts->fs, f->name, new_name);
    assert_no_err(err);

    // udpate our view
    strncpy(f->name, new_name, sizeof(f->name));
}

static void test_stat_verify(test_scenario *ts) {
    test_file *f = pick_random_file(ts);
    if (f == NULL) return;
    sfs_stat_info info;
    int err;

    log_action(ts, OP_STAT, f->name, NULL, -1, -1);

    if (f->exists) {
        err = ts->fs->stat(ts->fs, f->name, &info);
        assert_no_err(err);
        assert_size(f->size, info.file_size);
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

    log_action(ts, OP_TRUNC, f->name, NULL, -1, -1);

    int err = ts->fs->truncate(ts->fs, f->name);
    assert_no_err(err);

    // verify
    sfs_stat_info info;
    err = ts->fs->stat(ts->fs, f->name, &info);
    assert_no_err(err);
    assert_size(0, info.file_size);

    // update our view
    f->size = 0;
}

static void test_list_verify(test_scenario *ts) {
}

// ------------------------------------------------------------

static void verify_all_files(test_scenario *ts) {
    printf("Verifying all files\n");
    sfs_stat_info info;
    int err;

    for (int i = 0; i < MAX_TRACKED_FILES; i++) {
        test_file *f = &ts->files[i];
        if (f->name[0] == 0) continue;

        if (f->exists) {
            // verify it exists, check length, contents
            err = ts->fs->stat(ts->fs, f->name, &info);
            assert_no_err(err);
            assert_size(f->size, info.file_size);

        } else {
            // verify it does not exist
            err = ts->fs->stat(ts->fs, f->name, &info);
            assert(err == ERR_NOT_FOUND);
        }
    }
}

// ------------------------------------------------------------

// this runs the scenario
void run_scenario(unsigned int seed, int steps, int desired_block_size) {
    printf("Running scenario %d, %d steps, desired block_size %d\n", seed, steps, desired_block_size);
    initialize_log();


    mem_allocator *mem = new_malloc_based_mem_allocator();
    sector_device *dev = new_mem_based_sector_device(512, 65536); // 32 MB disk
    clock_device *clk = new_fixed_clock_device(123);
    simple_filesystem *fs = new_simple_filesystem(mem, dev, clk);
    int err;
    err = fs->mkfs(fs, "TEST", desired_block_size);
    assert_no_err(err);
    fs->mount(fs, 0);

    test_scenario scenario = { .seed = seed, .file_count = 0, .fs = fs };
    test_scenario *ts = &scenario;


    for (int i = 0; i < steps; i++) {
        test_operation op = random_operation(ts);
        ts->step_no = i;
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

    run_scenario(1234567, 10000, 512);
    run_scenario(1234567, 10000, 1024);
    run_scenario(1234567, 10000, 2048);
    run_scenario(1234567, 10000, 4096);

    // int seed = 1234567;
    // for (int i = 0; i < 10; i++) {
    //     int scenario_seed = rand_r(&seed);
    //     run_scenario(1234567, 1000, 512);
    // }
    return 0;
}

// ------------------------------------------------------------
