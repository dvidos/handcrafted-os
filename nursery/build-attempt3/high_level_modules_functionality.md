# high level modules functionality

this from GPT, to give an idea of what subsystems could support in the kernel.
How this hangs together:

* Syscalls call fs.*, proc.*, mm.*
* fs delegates path resolution to vfs
* vfs calls filesystem drivers
* filesystems talk to devices
* devices talk to hardware
* mm underpins everything
* No circular dependencies if done right.


## fs.h — Files as kernel resources (what users / libc see)

```c
// --------------------------------------------------------------
// fs.h — Files as kernel resources (what users / libc see)
// This is the layer syscalls bind to.
// Structs are public ABI, should be stable.
// --------------------------------------------------------------

struct file_t;      // An open file, has offset, vfs_node,
struct stat;        // Portable file metadata, ABI struct, what "ls -l" needs
struct dirent;      // Portable file metadata, ABI struct


// File lifecycle
int fs_open(const char *path, int flags, int mode);
int fs_close(file_t f);
int fs_dup(file_t f);
int fs_dup2(file_t oldf, file_t newf);

// I/O
ssize_t fs_read(file_t f, void *buf, size_t n);
ssize_t fs_write(file_t f, const void *buf, size_t n);
ssize_t fs_pread(file_t f, void *buf, size_t n, off_t off);
ssize_t fs_pwrite(file_t f, const void *buf, size_t n, off_t off);

// Positioning
off_t fs_seek(file_t f, off_t off, int whence);
off_t fs_tell(file_t f);

// Metadata
int fs_stat(const char *path, struct stat *out);
int fs_fstat(file_t f, struct stat *out);
int fs_chmod(const char *path, mode_t mode);
int fs_chown(const char *path, uid_t uid, gid_t gid);

// Structure
int fs_mkdir(const char *path, mode_t mode);
int fs_rmdir(const char *path);
int fs_unlink(const char *path);
int fs_rename(const char *oldpath, const char *newpath);

// Directories
int fs_opendir(const char *path);
int fs_readdir(int dirfd, struct dirent *out);
int fs_closedir(int dirfd);

// Mounting (policy side)
int fs_mount(const char *source, const char *target, const char *fstype);
int fs_umount(const char *target);
```


## vfs.h — Virtual filesystem (mechanism & glue)

```C
// --------------------------------------------------------------
// 2. vfs.h — Virtual filesystem (mechanism & glue)
// This is the core abstraction.
// Key point: Every filesystem implements vfs_ops, nothing else.
// --------------------------------------------------------------


// Core objects
struct vfs_node;    // A resolved filesystem object, holds node type, vfs ops, priv_data etc
struct vfs_inode;   // Filesystem-specific persistent metadata, size, perms, block ranges, etc
struct vfs_dentry;  // A name to node association. Holds name, parent dentry, vfs_node pointer.
struct vfs_mount;   // An instance of a mounted FS (path, fs ops etc)
struct vfs_fs;      // A filesystem type, holds name, operations, mount func.

// Filesys behavior / Node operations (minimum realistic set)
struct vfs_ops {
    int     (*lookup)(struct vfs_node *, const char *, struct vfs_node **);
    int     (*create)(struct vfs_node *, const char *, int mode);
    int     (*unlink)(struct vfs_node *, const char *);
    int     (*mkdir)(struct vfs_node *, const char *, int mode);
    int     (*rmdir)(struct vfs_node *, const char *);

    ssize_t (*read)(struct vfs_node *, off_t, void *, size_t);
    ssize_t (*write)(struct vfs_node *, off_t, const void *, size_t);

    int     (*truncate)(struct vfs_node *, off_t);
    int     (*stat)(struct vfs_node *, struct stat *);

    int     (*open)(struct vfs_node *);
    int     (*close)(struct vfs_node *);
};

// Filesystem registration
int vfs_register_fs(struct vfs_fs *fs);
int vfs_unregister_fs(struct vfs_fs *fs);

// Mounting
int vfs_mount(const char *path, struct vfs_fs *fs, void *fs_data);
int vfs_umount(const char *path);

// Path resolution
int vfs_resolve(const char *path, struct vfs_node **out);

// Reference management
void vfs_node_get(struct vfs_node *);
void vfs_node_put(struct vfs_node *);
```

## mm.h — Memory management (absolute authority)

```C
// -----------------------------------------------------------
// 3. mm.h — Memory management (absolute authority)
// -----------------------------------------------------------

// Kernel heap
void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void  kfree(void *ptr);

// Physical memory
phys_addr_t mm_alloc_frame(void);
void        mm_free_frame(phys_addr_t);

size_t      mm_total_memory(void);
size_t      mm_free_memory(void);

// Virtual memory
int mm_map(virt_addr_t v, phys_addr_t p, int flags);
int mm_unmap(virt_addr_t v);

int mm_protect(virt_addr_t v, int flags);

// Address spaces
struct vm_space;    // an address space, owns page tables and mappings, pagedir pointer, VM regions etc
struct vm_range;    // a contiguous virtual memory range

struct vm_space *mm_space_create(void);
void mm_space_destroy(struct vm_space *);

int mm_space_switch(struct vm_space *);

// User memory
int copy_to_user(void *dst, const void *src, size_t n);
int copy_from_user(void *dst, const void *src, size_t n);
```


## proc.h — Processes, threads, IPC

```c
// ------------------------------------------------
// 4. proc.h — Processes, threads, IPC
// ------------------------------------------------

struct proc;    // a unix-style process, has PID, threads, files, memory, signal handlers, etc
struct thread;  // a schedulable execution unit, has cpu registers, stack pointer, 

// Process lifecycle
pid_t proc_create(const char *name);
int   proc_exit(int status);
pid_t proc_wait(pid_t pid, int *status);

// Threads
tid_t thread_create(void (*entry)(void *), void *arg);
void  thread_exit(void);
void  thread_yield(void);

// Scheduling
void sched_sleep(uint64_t ticks);
void sched_wakeup(tid_t tid);

// Identity
pid_t proc_current(void);
tid_t thread_current(void);

// Signals / async events
int proc_kill(pid_t pid, int signal);
int signal_register(int sig, void (*handler)(int));

// IPC (baseline)
int pipe_create(int fds[2]);
int ipc_send(pid_t to, const void *msg, size_t len);
int ipc_recv(pid_t from, void *buf, size_t len);
```

## device.h — Hardware abstraction

```c
// ----------------------------------------------------
// 5. device.h — Hardware abstraction
// ----------------------------------------------------

// Device model
typedef int dev_t;

enum device_type {
    DEV_BLOCK,
    DEV_CHAR,
    DEV_NET,
    DEV_VIRTUAL
};

// Device operations
struct device_ops {
    int     (*open)(void);
    int     (*close)(void);

    ssize_t (*read)(void *, size_t);
    ssize_t (*write)(const void *, size_t);

    int     (*ioctl)(int cmd, void *arg);
};

// Registration
int device_register(dev_t id,
                    enum device_type type,
                    struct device_ops *ops,
                    void *priv);
int device_unregister(dev_t id);

// Lookup
struct device_ops *device_get_ops(dev_t id);
void *device_get_private(dev_t id);
```
