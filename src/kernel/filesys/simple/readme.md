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

---

## Possible structure of the description

1. split the device into blocks, decide block size, find ways to read/write blocks.
1. impement a caching layer on top of the blocks, tracking dirty blocks. use LRU as needed.
1. first block allocated to superblock, write information for the whole filesystem.
1. some subsequent blocks devoted for bitmap to track used blocks. one bit for each block.
1. then.... inodes,
   - they use ranges/extents to track which blocks contain file data.
   - can have direct, indirect, double indirect ranges
   - they track file_size, mtime, permissions, gid/uid etc
   - make functions to read/write/truncate/extend inode file contents
1. implement caching layer for inodes, track dirty, evict as needed etc.
1. then, use the inode actions to implement
   - one file used for storing inodes. inode_id is the rec_no
   - one file used for root directory, entries have name & inode_id.
   - make functions to read/append/write/delete inodes and dir-entries.
1. make functions to resolve a path to a cached inode.
1. implement caching layer for open files, one or more per inode. they track file position.
1. from there, create the external API facing functions.  

Some key points:

* The whole thing is primarily an in-memory structure, 
* Some parts persisted to disk periodically, on sync, on unmount.
