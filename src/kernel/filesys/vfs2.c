#include <ctypes.h>
#include <lock.h>
#include <errors.h>
#include <filesys/drivers.h>
#include <filesys/partition.h>
#include <filesys/vfs.h>
#include <filesys/mount.h>
#include <klib/path.h>
#include <klib/string.h>

/*

// attempt to simplify, based on https://tldp.org/LDP/khg/HyperNews/get/fs/vfstour.html
// we should implement a read/write cache as well, at some point in the future

struct fs_driver;
struct partition;
struct superblock;
struct file_descriptor;
struct open_file;

typedef struct fs_driver fs_driver_t;
typedef struct partition partition_t;
typedef struct superblock superblock_t;

struct fs_driver_ops {
    // read partition and verify if the filesystem is supported
    int (*supported)(partition_t *partition);

    // read partition and populate superblock
    int (*mount)(partition_t *partition, char *options, superblock_t *superblock);

    // write superblock and data needed for a fresh filesystem
    int (*mkfs)(partition_t *partition, char *options);
};

struct superblock {
    partition_t *partition; // reference
    fs_driver_t *driver;  // reference

    // owned and managed by filesystem driver
    lock_t lock;
    struct filesys_ops *ops;
    void *fs_private_data; // owned by fs driver
};

// DAMN IT!!! it's not simple:
// we need to update file size and mtime upon close,
// so each file needs to point to the including directory, so we can find it.
// but for ext2, this is part of the inode data, not the containing directory,
// it is done via the inode_write() call
// so these two filesystems are not that similar...
// the decision for a descriptor to point to owning directory is affecting VFS design....
// and then, how do we free them???
struct filesys_ops {
    // return the root dir designator. not free()'d
    file_descriptor_t *(*root_dir)();

    // find a name in a directory, and populate a file_descriptor
    int (*lookup)(file_descriptor_t *dir, char *name, file_descriptor_t **result);

    // open file for reading or writing, populate file
    int (*open)(file_descriptor_t *dir, int flags, file_t **file);

    // move the position in the file, return new position or negative error
    int (*seek)(file_t *file, int offset, int origin);

    // read from file, returns bytes read or negative error
    int (*read)(file_t *file, char *buffer, int length);

    // write to file, returns bytes read or negative error
    int (*write)(file_t *file, char *buffer, int length);

    // flush the file caches
    int (*flush)(file_t *file);

    // flush and close the file, free resources
    int (*close)(file_t *file);

    // open dir for enumerating
    int (*opendir)(file_descriptor_t *dir, file_t **open_dir);

    // get the next entry from the directory list
    int (*readdir)(file_t *open_dir, file_descriptor_t *entry);

    // close the directory handle, free resources
    int (*closedir)(file_t *open_dir);

    // create a normal file (or whatever the fs supports)
    int (*create)(file_descriptor_t *parent_dir, char *name);

    // delete a file or directory
    int (*unlink)(file_descriptor_t *parent_dir, char *name);

    // create a directory
    int (*mkdir)(file_descriptor_t *parent_dir, char *name);

    // create a directory
    int (*rmdir)(file_descriptor_t *parent_dir, char *name);

    // populate stats of the filesystem
    int (*stats)(superblock_t *superblock, struct fs_info *info);

    // sync file system (flush pending caches)
    int (*sync)(superblock_t *superblock);

    // close superblock, free owned resources
    int (*unmount)(superblock_t *superblock);
};

struct fs_info {
    uint32_t total_size;
    uint32_t used_size;
    uint32_t free_size;
    uint32_t files_count;
    uint32_t dirs_count;
};

// ----- VFS -----------------------------------------------


// similar to namei() in unix
static int _resolve_path_to_descriptor(const char *path, const file_descriptor_t *root_dir, const file_descriptor_t *curr_dir, bool containing_folder, file_descriptor_t **target) {
    // test edge cases
    if (path == NULL)
        return ERR_BAD_ARGUMENT;
    if (*path == '\0')
        return ERR_BAD_ARGUMENT;
    if (root_dir == NULL)
        return ERR_NO_FS_MOUNTED;
    if (curr_dir == NULL)
        return ERR_BAD_ARGUMENT;

    // easy cases to get out of the way fast.
    if (strlen(path) == 1) {
        if (*path == '/') {
            *target = clone_file_descriptor(root_dir);
            return SUCCESS;
        } else if (*path == '.') {
            *target = clone_file_descriptor(curr_dir);
            return SUCCESS;
        }
    }

    // duplicate so we may go to containing folder, tokenize and parse
    char *work_path = strdup(path);
    if (containing_folder) {
        // we could have cases "./asdf", "asdf" or "/asdf"
        work_path = dirname(work_path);
        if (strlen(work_path) == 1 && *work_path == '.') {
            *target = clone_file_descriptor(curr_dir);
            kfree(work_path);
            return SUCCESS;
        }
        else if (strlen(work_path) == 1 && *work_path == '/') {
            *target = clone_file_descriptor(root_dir);
            kfree(work_path);
            return SUCCESS;
        }
    }

    // establish base dir
    file_descriptor_t *base_dir = NULL;
    if (*path == '/') {
        base_dir = clone_file_descriptor(root_dir);
        path++;
    } else {
        base_dir = clone_file_descriptor(curr_dir);
    }
    
    // maybe we were given "/mnt/hdd2/dir/file.txt" and we are at the "hdd2"
    // how to see that this is a mounted file system?
    // so that we switch to the new superblock and the correct function pointers?
    // maybe each filesystem driver will maintain a list of mounts and directories,
    // e.g. a list of file_designator_t and mounts,
    // so that resolving such a directory, we are given the root directory
    // of the mounted file system.
    // but, to not implement this in every filesys driver, 
    // we must implement something on VFS level. 

    int err = SUCCESS;
    char *name = strtok(work_path, "/");
    while (true) {

        // there's another part to look for, verify that 
        // we are searching inside a directory, not a file
        if ((base_dir->flags && FD_DIR) == 0) {
            err = ERR_NOT_A_DIRECTORY;
            goto out;
        }

        // we are now sure we are based in a directory.
        err = base_dir->superblock->ops->lookup(base_dir, name, target);
        if (err) goto out;

        // here we could translate for mounted filesystems.
        // one can think of a mount as two pairs of file descriptors:
        // one for the mount point directory of the host system,
        // one for the root directory of the hosted system.
        // so, if we got a descriptor pointing to one dir (e.g. fs A, "/mnt/hda")
        // we could substitute the other dir (fs B, "/")

        // let's check if we finished
        name = strtok(NULL, "/");
        if (name == NULL || strlen(name) == 0) {
            // we are done, target contains the... target, so free base_dir
            destroy_file_descriptor(base_dir);
            break;
        }

        // we will search again, so rebase on the new target.
        // lookup() will create a new file_descriptor each call
        // so, free our current, use the new one.
        destroy_file_descriptor(base_dir);
        base_dir = *target;
    }

    // we visited all levels, we should be ok.
    err = SUCCESS;
out:
    kfree(work_path);
    return err;
}

int vfs_read(file_t *file, char *buffer, int length) {
    if (file->superblock->ops->read == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->read(file, buffer, length);
}

int vfs_write(file_t *file, char *buffer, int length) {
    if (file->superblock->ops->write == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->write(file, buffer, length);
}

int vfs_seek(file_t *file, int offset, enum seek_origin origin) {
    if (file->superblock->ops->seek == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->seek(file, offset, origin);
}

int vfs_close(file_t *file, char *buffer, int length) {
    if (file->superblock->ops->close == NULL)
        return ERR_NOT_SUPPORTED;
    return file->superblock->ops->close(file);
}

int vfs_opendir(file_descriptor_t *dir, file_t *open_dir) {
    if (dir->superblock->ops->opendir == NULL)
        return ERR_NOT_SUPPORTED;
    return dir->superblock->ops->opendir(dir, open_dir);
}

int vfs_readdir(file_t *open_dir, file_descriptor_t *entry) {
    if (open_dir->superblock->ops->readdir == NULL)
        return ERR_NOT_SUPPORTED;
    return open_dir->superblock->ops->readdir(open_dir, entry);
}

int vfs_closedir(file_t *open_dir) {
    if (open_dir->superblock->ops->closedir == NULL)
        return ERR_NOT_SUPPORTED;
    return open_dir->superblock->ops->closedir(open_dir);
}



// ----- Sample filesystem -----------------------------------------------

// in order to write a file system:
// - write a driver to offer a few methods (supported, mount)
// - write a few more functions based on supports
// maybe maintain a memory of open files.

// ideas from cmocka: https://github.com/google/cmockery/blob/master/docs/user_guide.md
// define malloc() to __test_malloc() etc, to detect memory leaks in tests
// assert_null(x), assert_not_null(x), assert_int_equal(i, 10), etc
// tests are invocated with a call to "run_tests()"", by passing
// an initialized array of calls to unit_test, by passing a function pointer.
 
// mocks of return values are setup by calling: `will_return(function, value)`
// the mock function will use a `return (typecast)mock();` construct
// other macros for setting up and validating arguments

int mockfs_supported(partition_t *partition);
int mockfs_mount(partition_t *partition, char *options, superblock_t *superblock);
int mockfs_mkfs(partition_t *partition, char *options);

file_descriptor_t *mockfs_root_dir();
int mockfs_lookup(file_descriptor_t *dir, char *name, file_descriptor_t **result);
int mockfs_open(file_descriptor_t *dir, int flags, file_t **file);
int mockfs_seek(file_t *file, int offset, int origin);
int mockfs_read(file_t *file, char *buffer, int length);
int mockfs_write(file_t *file, char *buffer, int length);
int mockfs_flush(file_t *file);
int mockfs_close(file_t *file);
int mockfs_opendir(file_descriptor_t *dir, file_t **open_dir);
int mockfs_readdir(file_t *open_dir, file_descriptor_t *entry);
int mockfs_closedir(file_t *open_dir);
int mockfs_create(file_descriptor_t *parent_dir, char *name);
int mockfs_unlink(file_descriptor_t *parent_dir, char *name);
int mockfs_mkdir(file_descriptor_t *parent_dir, char *name);
int mockfs_rmdir(file_descriptor_t *parent_dir, char *name);
int mockfs_stats(superblock_t *superblock, struct fs_info *info);
int mockfs_sync(superblock_t *superblock);
int mockfs_unmount(superblock_t *superblock);


// read partition and verify if the filesystem is supported
int mockfs_supported(partition_t *partition) {
    return ERR_NOT_SUPPORTED;
}
// read partition and populate superblock
int mockfs_mount(partition_t *partition, char *options, superblock_t *superblock) {
    return ERR_BAD_VALUE;
}
// write superblock and data needed for a fresh filesystem
int mockfs_mkfs(partition_t *partition, char *options) {
    return ERR_NOT_IMPLEMENTED;
}
// return the root dir designator. not free()'d
file_descriptor_t *mockfs_root_dir() {
    return NULL;
}
// find a name in a directory, and populate a file_descriptor
int mockfs_lookup(file_descriptor_t *dir, char *name, file_descriptor_t **result) {
    return ERR_NOT_IMPLEMENTED;
}
// open file for reading or writing, populate file
int mockfs_open(file_descriptor_t *dir, int flags, file_t **file) {
    return ERR_NOT_IMPLEMENTED;
}
// move the position in the file, return new position or negative error
int mockfs_seek(file_t *file, int offset, int origin) {
    return ERR_NOT_IMPLEMENTED;
}
// read from file, returns bytes read or negative error
int mockfs_read(file_t *file, char *buffer, int length) {
    return ERR_NOT_IMPLEMENTED;
}
// write to file, returns bytes read or negative error
int mockfs_write(file_t *file, char *buffer, int length) {
    return ERR_NOT_IMPLEMENTED;
}
// flush the file caches
int mockfs_flush(file_t *file) {
    return ERR_NOT_IMPLEMENTED;
}
// flush and close the file, free resources
int mockfs_close(file_t *file) {
    return ERR_NOT_IMPLEMENTED;
}
// open dir for enumerating
int mockfs_opendir(file_descriptor_t *dir, file_t **open_dir) {
    return ERR_NOT_IMPLEMENTED;
}
// get the next entry from the directory list
int mockfs_readdir(file_t *open_dir, file_descriptor_t *entry) {
    return ERR_NOT_IMPLEMENTED;
}
// close the directory handle, free resources
int mockfs_closedir(file_t *open_dir) {
    return ERR_NOT_IMPLEMENTED;
}
// create a normal file (or whatever the fs supports)
int mockfs_create(file_descriptor_t *parent_dir, char *name) {
    return ERR_NOT_IMPLEMENTED;
}
// delete a file or directory
int mockfs_unlink(file_descriptor_t *parent_dir, char *name) {
    return ERR_NOT_IMPLEMENTED;
}
// create a directory
int mockfs_mkdir(file_descriptor_t *parent_dir, char *name) {
    return ERR_NOT_IMPLEMENTED;
}
// create a directory
int mockfs_rmdir(file_descriptor_t *parent_dir, char *name) {
    return ERR_NOT_IMPLEMENTED;
}
// populate stats of the filesystem
int mockfs_stats(superblock_t *superblock, struct fs_info *info) {
    return ERR_NOT_IMPLEMENTED;
}
// sync file system (flush pending caches)
int mockfs_sync(superblock_t *superblock) {
    return ERR_NOT_IMPLEMENTED;
}
// close superblock, free owned resources
int mockfs_unmount(superblock_t *superblock) {
    return ERR_NOT_IMPLEMENTED;
}
struct fs_driver_ops mockfs_driver_ops = {
    .supported = mockfs_supported,
    .mount = mockfs_mount,
    .mkfs = mockfs_mkfs
};
struct filesys_ops mockfs_filesys_ops = {
    .root_dir = mockfs_root_dir,
    .lookup   = mockfs_lookup,
    .open     = mockfs_open,
    .seek     = mockfs_seek,
    .read     = mockfs_read,
    .write    = mockfs_write,
    .flush    = mockfs_flush,
    .close    = mockfs_close,
    .opendir  = mockfs_opendir,
    .readdir  = mockfs_readdir,
    .closedir = mockfs_closedir,
    .create   = mockfs_create,
    .unlink   = mockfs_unlink,
    .mkdir    = mockfs_mkdir,
    .rmdir    = mockfs_rmdir,
    .stats    = mockfs_stats,
    .sync     = mockfs_sync,
    .unmount  = mockfs_unmount,
};

// ----- Tests to perform -------------------------------------------

void test_vfs() {
    // essentially, mock the filesystem driver to return specific responses
    // e.g. when lookup() returns a normal file, the VFS should return a NOT_A_DIRECTORY etc
    // essentially we can test how calls to VFS propagate to the driver.

    // for example, how make sure that _resolve_path_to_descriptor() works in all edge cases?
    // if path is '/', return the root dir
    // if path is '.', return the curr dir
    // if path is "/a/b" return the "b" designator
    // if path is "/a/b" and we want containing folder, return "a"
    // if path is "a" and we want containing folder, return curr_dir
    // if path is "/a" and we want containing folder, return root_dir
    // if path is "/usr/var/log/errors.txt", verify 4 calls etc.
    // dependencies are the superblock operations, curr dir and root dir.
    // another dependency is the current mounts mapping
    // also, tests can prove that mounted filesystems will be returned as needed

    // another thing is that, it feels that the VFS / filesys
    // maintain a list of open files, with their locks etc.
    // that's how it knows not to delete a directory, 
    // when someone is using it for their current directory...
    // this will be a whole new challenge!
}

void test_filesystem_a() {
    // create a memory storage device, create a simple partition in it.
    // then test a filesystem to create(), open() the partition,
    // and perform various operations (create dir, files, list, write, read, delete etc)

    // for example, do the following:
    // - mkfs, create file, write "hello" in file, 
    // - create directory, file in directory, delete file
    // at each step, verify underlying data is correct,
    // i.e. modification timestamps etc.
}
*/