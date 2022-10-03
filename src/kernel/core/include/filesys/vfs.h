#ifndef _VFS_H
#define _VFS_H

#include <ctypes.h>


struct file_timestamp {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

typedef struct dir_entry {
    char short_name[12+1];
    uint32_t file_size;
    struct {
        uint8_t label: 1;
        uint8_t dir: 1;
        uint8_t read_only: 1;
    } flags;
    struct file_timestamp created;
    struct file_timestamp modified;
} dir_entry_t;

struct file_ops;

typedef struct file {
    struct storage_dev *storage_dev;
    struct partition *partition;
    struct file_system_driver *driver;
    
    char *path; // relative to mount point
    // we should have a pointer to the dir_entry here!
    void *fs_driver_priv_data;
    struct file_ops *ops;
} file_t;

enum seek_origin {
    SEEK_START,
    SEEK_CURRENT,
    SEEK_END
};

// to implement
#define OPEN_RDWR       0x00  // allows both reading and writing (default)
#define OPEN_RDONLY     0x01  // disallows writing operations
#define OPEN_WRONLY     0x02  // disallows reading operations
#define OPEN_CREATE     0x04  // file created if not existing
#define OPEN_APPEND     0x08  // auto seeks end before each write
#define OPEN_TRUNC      0x10  // if allowed, truncate to zero len upon opening


struct file_ops {
    int (*opendir)(char *path, file_t *file);
    int (*rewinddir)(file_t *file);
    int (*readdir)(file_t *file, dir_entry_t *entry);
    int (*closedir)(file_t *file);

    int (*open)(char *path, file_t *file);
    int (*read)(file_t *file, char *buffer, int bytes);
    int (*write)(file_t *file, char *buffer, int bytes);
    int (*seek)(file_t *file, int offset, enum seek_origin origin);
    int (*close)(file_t *file);

    int (*touch)(char *path, file_t *file);
    int (*mkdir)(char *path, file_t *file);
    int (*unlink)(char *path, file_t *file);

    // version 2, using dir_entry as the equivaluent of an inode in unix 
    // (this may bite us later, when we implement ext2)

    // there should be two file structures:
    // entry_t   closed file information (size, cluster) - aka inode in linux
    // file_t    open file information (mode, position)  - aka FILE in linux
    
    // we don't work with paths, we work with entries
    // use an open dir to translate a path to an entry
    // using an entry we can opendir() the CWD.
    // so we can have either the root dir, or the CWD
    // to resolve a path into an entry.

    // all operations (read, write) work with entries
    // so that they don't need to resolve the path every time
    // they can use the cluster_no / inode_no or anything 
    // to affect files fast.

    // i may be naive, thinking i need only two objects,
    // linux today employs 4: superblock, inode, dentry, file.
    // maybe I need to delve deeper into ext2?
    // see https://unix.stackexchange.com/questions/4402/what-is-a-superblock-inode-dentry-and-a-file
    // and https://developer.ibm.com/tutorials/l-linux-filesystem/


    // // open root dir, without any path designated. populate file_t.
    // // find_dir_entry() and opendir() should be used to open the current directory.
    // int (*open_root_dir)(file_t *root_dir);

    // // resolve, based on the start dir
    // // should be fast, iterating on parts of the rel_path,
    // // could use either root dir or current working dir
    // int (*find_dir_entry)(file_t *start_dir, char *rel_path, fentry_t *fentry);

    // // open based on entry. entry should contain cluster_no already, so it should be fast.
    // int (*open)(fentry_t *fentry, file_t *file);

    // int (*opendir)(fentry_t *fentry,  file_t *file);
};

// lots of vfs info here: https://tldp.org/LDP/lki/lki-3.html
// given an already open dir, follow a path, return a "dir_entry"
// idea is that this has to be fast, dir_entry should include cluster no or similar.
// but, what about spanning to other mounted systems?

// a file system should support two initial things:
// - open root dir and return a fast access dir structure
// - given a fast access dir structure and a relative path,
//   return a fast access structure, either of a dir (e.g. CWD) or a file.



// -------------------------------------

int vfs_opendir(char *path, file_t *file);
int vfs_rewinddir(file_t *file);
int vfs_readdir(file_t *file, struct dir_entry *dir_entry);
int vfs_closedir(file_t *file);

int vfs_open(char *path, file_t *file);
int vfs_read(file_t *file, char *buffer, int bytes);
int vfs_write(file_t *file, char *buffer, int bytes);
int vfs_seek(file_t *file, int offset, enum seek_origin origin);
int vfs_close(file_t *file);

int vfs_touch(char *path);
int vfs_mkdir(char *path);
int vfs_unlink(char *path);


#endif
