# simple file system

## Simple file system, as a fun exercise.

- reading/writing in blocks. Each block can be 512, 1K, 2K or 4K.
    - because it's faster this way, instad of sectors alone
- first block (block 0) hosts superblock.
    - it contains one inode with a file of all number based inodes
    - it also contains one inode with the root directory
- subsequent blocks have the bitmap for which blocks are available and which are not.
    - bit=1, block is used, bit=0, block is free. 
    - e.g. 1 block of 4K tracks 32KB of blocks of 4K each, or 128MB of space!
    - one inode file

## File format

- file contents are one or more blocks.
- file metadata exists in inode structure (ranges, possibly extended)

## Directory format

- directory entries are stored in a file, like all others.
- entries consist of name and inode number (offset in the inodes database)

## Inodes

- structures that describe files.
- size, type, perms, owners, dates, etc
- tracks content in small array of ranges, pairs of first_block & blocks_count
- if internal array exhausted, one block can store many ranges (e.g. 64 ranges in a 512B block, or 512 ranges in a 4K block)
- all inodes stored in the inodes database file, held by the superblock

## Used / free space

- tracked in a large bitmap of fixed size, one bit per block 1=used, 0=free
- because linked list chains are so slow

## Opening files:

- when opening files we need to have:
    - in memory copy of the inode, along with a "is-dirty" flag, maybe open handles count (cached in filesys)
    - in memory file handle with pointer to file location, open flags, in-mem-inode reference (cached in apps)

---

actually, this points to the important internal functions of a file system:

- inode manipulation (+range manipulation for finding, extending, releasing)
- blocks manipulation (allocation, freeing)
- directory entries manipulation (resolve, find, add, remove, modify, skip/reuse empty slots)
- cached read/write operations
