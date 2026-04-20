typedef unsigned long size_t;




// simplest process mgmt, static array, round-robin scheduling
struct proc {
    int status;
    size_t memory_start;
    size_t memory_size;
};
#define MAX_PROCESSES    32
struct proc processes[MAX_PROCESSES];
void schedule() { ; } // called every 100ms


// no memory protection/paging, each process given an area
int pmm_allocate_page();
void pmm_release_page(int page_no);


// block devices (e.g. ttys)
struct char_device {
    int (*read)(int handle, char *buffer, int size);
    int (*write)(int handle, char *buffer, int size);
};
#define MAX_DEVICES    8
struct char_device dev_drivers[MAX_DEVICES];



// file system: based on blocks of 512 bytes
// block 0:    boot
// block 1:    superblock
// blocks 2-n: inodes (fixed num)
// blocks n+1: data
// inode: type (can be file, dir, dev, symlink, etc), 10 direct blocks, 1 indirect, 1 dbl-indirect, 1 trpl-indirect
// inodes + dir entries of 32 or 64 bytes



// syscall is simple: read, write, open, close, fork, exec, wait
