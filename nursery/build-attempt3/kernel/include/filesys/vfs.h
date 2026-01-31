#ifndef _VFS_H
#define _VFS_H

#include <ctypes.h>
#include <lock.h>
#include <filesys/drivers.h>
#include <filesys/partition.h>



// structure representing an "opened" filesystem
typedef struct superblock {
    struct filesys_driver *driver;
    struct partition *partition;
    struct file_ops *ops;
    lock_t write_lock;
    void *priv_fs_driver_data;
} superblock_t;



// file_descriptor->flags
#define FD_FILE   1
#define FD_DIR    2

// structure representing a pointer to a file, somewhere on disk
// this structure to be treated as a value object,
// we want to copy it, clone it, compare it etc.
// this allows VFS to make decisions, e.g. to hop directories etc.
// it must be entirely managed by VFS (i.e. have no private structure of unknown size)
// so that ic can be used for over-filesys ceoncerns, such as the mounting table.
typedef struct file_descriptor {
    // just reference
    struct superblock *superblock; 

    // needed to update file size and mtime on file close, NULL for root dir
    // cloned and freed.
    struct file_descriptor *owning_directory; 

    char *name; // file name, not full path (owned, cloned, free etc)
    uint32_t location; // e.g. inode for ext2, cluster_no for FAT
    uint32_t size;
    uint32_t flags;
    uint32_t ctime; // create time since 1970
    uint32_t mtime; // modified time since 1970
} file_descriptor_t;


file_descriptor_t *create_file_descriptor(superblock_t *superblock, const char *name, uint32_t location, file_descriptor_t *owning_dir);
file_descriptor_t *clone_file_descriptor(const file_descriptor_t *fd);
void copy_file_descriptor(file_descriptor_t *dest, const file_descriptor_t *source);
bool file_descriptors_equal(const file_descriptor_t *a, const file_descriptor_t *b);
void destroy_file_descriptor(file_descriptor_t *fd);
void debug_file_descriptor(const file_descriptor_t *fd, int depth);
int file_descriptor_get_full_path_length(const file_descriptor_t *fd);
void file_descriptor_get_full_path(const file_descriptor_t *fd, char **full_path); // caller to free path

typedef struct file {
    struct superblock *superblock;
    struct file_descriptor *descriptor;
    const void *fs_driver_private_data;
} file_t;


file_t *create_file_t(superblock_t *superblock, file_descriptor_t *descriptor);
void debug_file_t(file_t *file);
void destroy_file_t(file_t *file);





struct file_timestamp {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
};

typedef struct dir_entry {

    // linking back to where this entry will reside
    struct superblock *superblock;

    // filesystem specific, inode for ext2, cluster_no for FAT etc
    // using this instead of a pointer to private data, 
    // as we want this structure to not need nested free()
    // this is to allow fast opening of a file without path re-discovery
    uint32_t location_in_dev;

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



enum seek_origin {
    SEEK_START,
    SEEK_CURRENT,
    SEEK_END
};



// keep same with stdio.h in libc
#define DE_TYPE_DIR   1
#define DE_TYPE_FILE  2

typedef struct dirent {
    char name[256];
    uint8_t  type;
    uint32_t size;
    uint32_t location;
} dirent_t;

// to implement
#define OPEN_RDWR       0x00  // allows both reading and writing (default)
#define OPEN_RDONLY     0x01  // disallows writing operations
#define OPEN_WRONLY     0x02  // disallows reading operations
#define OPEN_CREATE     0x04  // file created if not existing
#define OPEN_APPEND     0x08  // auto seeks end before each write
#define OPEN_TRUNC      0x10  // if allowed, truncate to zero len upon opening


struct file_ops {

    // get root descriptor, not freed, can point to static data
    int (*root_dir_descriptor)(struct superblock *sb, file_descriptor_t **fd);

    // find a name in a directory, and populate a file_descriptor
    int (*lookup)(file_descriptor_t *dir, char *name, file_descriptor_t **result);

    // open file for reading or writing, populate file
    int (*open)(file_descriptor_t *fd, int flags, file_t **file);

    // move the position in the file, return new position or negative error
    int (*seek)(file_t *file, int offset, enum seek_origin origin);

    // read from file, returns bytes read or negative error
    int (*read)(file_t *file, char *buffer, int length);

    // write to file, returns bytes read or negative error
    int (*write)(file_t *file, char *buffer, int length);

    // flush the file caches
    int (*flush)(file_t *file);

    // flush and close the file, free resources
    int (*close)(file_t *file);

    // open dir for enumerating entries
    int (*opendir)(file_descriptor_t *dir, file_t **open_dir);

    // get the next entry from the directory list
    int (*rewinddir)(file_t *open_dir);

    // get the next entry from the directory list
    int (*readdir)(file_t *open_dir, file_descriptor_t **fd);

    // close the directory handle, free resources
    int (*closedir)(file_t *open_dir);

    // create a file (or refresh access time)
    int (*touch)(file_descriptor_t *parent_dir, char *name);

    // unlink a file
    int (*unlink)(file_descriptor_t *parent_dir, char *name);

    // make a directory (maybe the . and .. as well?)
    int (*mkdir)(file_descriptor_t *parent_dir, char *name);

    // remove a directory (maybe the . and .. as well?)
    int (*rmdir)(file_descriptor_t *parent_dir, char *name);
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

// resolves a relative or absolute path to a descriptor. caller to destroy.
int vfs_resolve(const char *path, const file_descriptor_t *root_dir, const file_descriptor_t *curr_dir, bool containing_folder, file_descriptor_t **target);

int vfs_open(char *path, file_t **file);
int vfs_read(file_t *file, char *buffer, int bytes);
int vfs_write(file_t *file, char *buffer, int bytes);
int vfs_seek(file_t *file, int offset, enum seek_origin origin);
int vfs_flush(file_t *file);
int vfs_close(file_t *file);

int vfs_opendir(char *path, file_t **file);
int vfs_rewinddir(file_t *file);
int vfs_readdir(file_t *file, file_descriptor_t **fd); // caller must destroy fd
int vfs_closedir(file_t *file);

int vfs_touch(char *path);
int vfs_unlink(char *path);
int vfs_mkdir(char *path);
int vfs_rmdir(char *path);


#endif
