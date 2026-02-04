# filesys

There's at least three distinct layers for file system:

* The FS driver, for discrete file system types, operating with the disk blocks (fat, ext2, sfs etc)
* The VFS, or virtual file system, that provides a single tree view, with mounted drivers
* The FS, the high level functions (open, read, write) that use the VFS. These are what syscall uses.

Note that the drivers and FS depend on the VFS, not vice versa. This means
that there are core structs in VFS that others must use.

* VFS speaks in descriptors and open handles.
* FS drivers speak in on-disk objects and blocks. Doesn't know paths. Doesn't store VFS structs

Naming convetions:

* `file_descriptor` is the description of a closed file (e.g. inode)
* `open_file` is the representation of an open file (e.g. mode, position, etc)


## responsibilities

The driver provides

* mount(), unmount()
* get_root_file_descriptor()  // get root dir descriptor
* lookup_file_descriptor()    // using a dir descriptor and name, get a file descriptor
* open(), close()             // given a descriptor, get an open_file
* read(), write(), flush()
* opendir(), readdir(), closedir()   // ability to get dir entries
* create(), unlink() (mkdir(), rmdir())
* stat(), truncate(), sync()

```c
typedef struct fs_driver_ops {
    int (*mount)(struct superblock *sb);
    int (*unmount)(struct superblock *sb);
    int (*get_root_dir)(struct superblock *sb, file_descriptor_t **root_dir);
    int (*lookup)(file_descriptor_t *dir, const char *name, file_descriptor_t **out);
    int (*open)(file_descriptor_t *fd, int flags, file_t *file);
    int (*close)(file_t *file);
    int (*read)(file_t *file, void *buf, size_t len);
    int (*write)(file_t *file, const void *buf, size_t len);
    int (*flush)(file_t *file);
    int (*opendir)(file_descriptor_t *dir, file_t *dir_handle);
    int (*readdir)(file_t *dir_handle, file_descriptor_t **out);
    int (*rewinddir)(file_t *dir_handle);
    int (*closedir)(file_t *dir_handle);
    int (*create)(file_descriptor_t *parent, const char *name, int type, file_descriptor_t **out);
    int (*unlink)(file_descriptor_t *parent, const char *name);
    int (*mkdir)(file_descriptor_t *parent, const char *name); // dirs have special create semantics
    int (*rmdir)(file_descriptor_t *parent, const char *name); // dirs have special delete semantics
    int (*stat)(file_descriptor_t *fd, struct stat *out);
    int (*truncate)(file_descriptor_t *fd, size_t size);
    int (*sync)(struct superblock *sb);
} fs_driver_ops_t;

typedef struct superblock {       // lives for duration of mount()
    fs_driver_ops_t *driver;      // plugin contract
    struct block_device *bdev;    // partition / disk
    void *fs_private_data;        // FS-specific superblock data
    int fs_id;                    // unique mount id (= global_monotonic_counter++)
    lock_t lock;                  // protects fs-level metadata
} superblock_t;

typedef struct file_descriptor {  // value object, copiable, cacheable, can test for equality
    superblock_t *sb;             // which mounted FS
    uint64_t inode;               // inode / cluster / object id
    uint32_t type;                // file, dir, symlink
    uint32_t mode;                // permissions
    uint64_t size;                // file size in bytes
    uint64_t blocks;              // allocated blocks
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    // path resolution support (optional but useful)
    struct file_descriptor *parent;  // owned copy or NULL
    char *name;                      // owned
} file_descriptor_t;

typedef struct file_t {           // vfs-owned, one per open handle, created/destroyed in vfs_open()/vfs_close()
    superblock_t *sb;
    file_descriptor_t *desc;      // immutable identity
    uint64_t offset;              // VFS-owned file position
    uint32_t flags;               // RDONLY, WRONLY, APPEND, etc
    void *fs_private_data;        // driver-specific open context
    lock_t lock;                  // protects offset & state
} file_t;

struct stat {            // posix thing, exported to libc and apps
    uint64_t st_dev;     // filesystem id (sb.fs_id)
    uint64_t st_ino;     // inode number
    uint32_t st_mode;    // file type + permissions
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_size;    // file size in bytes
    uint64_t st_blocks;  // number of blocks
    uint32_t st_blksize; // block size
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

// ----------- Open flags below -----------------------

// Access mode (exactly one must be set)
#define O_RDONLY    0x0000  // open for read only
#define O_WRONLY    0x0001  // open for write only
#define O_RDWR      0x0002  // open for read and write
#define O_ACCMODE   0x0003  // mask to extract access mode
// Creation / open behavior
#define O_CREAT     0x0040  // create file if it does not exist
#define O_EXCL      0x0080  // with O_CREAT, fail if file already exists
#define O_TRUNC     0x0200  // truncate file to zero length on open
#define O_APPEND    0x0400  // force writes to append at end of file
// Open-time behavior modifiers (optional / future)
#define O_NONBLOCK  0x0800  // do not block on open/read/write
#define O_SYNC      0x1000  // writes complete before returning
#define O_CLOEXEC   0x2000  // close file on exec()

// ----------------- File type flags below ----------------------

// used by create(), mkdir(), mknod(), stat.st_mode
// File type (pick only one)
#define S_IFMT   0170000  // mask for file type bits
#define S_IFREG  0100000  // regular file
#define S_IFDIR  0040000  // directory
#define S_IFCHR  0020000  // character device
#define S_IFBLK  0060000  // block device
#define S_IFIFO  0010000  // FIFO / named pipe
#define S_IFLNK  0120000  // symbolic link
// Owner permissions
#define S_IRUSR  00400  // owner can read
#define S_IWUSR  00200  // owner can write
#define S_IXUSR  00100  // owner can execute
// Group permissions
#define S_IRGRP  00040  // group can read
#define S_IWGRP  00020  // group can write
#define S_IXGRP  00010  // group can execute
// Others permissions
#define S_IROTH  00004  // others can read
#define S_IWOTH  00002  // others can write
#define S_IXOTH  00001  // others can execute
```

A driver is “complete enough” when it can:

* return a root descriptor
* lookup a name in a directory
* open a file
* read and write at an offset
* enumerate directories
* update metadata on close
* create / unlink / mkdir / rmdir / mknod

Everything else (stat, fstat, lseek, dup, exec) comes for free at VFS level.
