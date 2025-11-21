#include "internal.h"


static int read_inode_from_inodes_database(mounted_data *mt, int rec_no, inode_in_mem *inmem) {
    int err;

    inode *db = &mt->superblock->inodes_db_inode;
    uint32_t inodes_in_file = db->file_size / sizeof(inode);
    if (rec_no >= inodes_in_file)
        return ERR_OUT_OF_BOUNDS;

    uint32_t inodes_per_block = mt->superblock->block_size_in_bytes / sizeof(inode);
    uint32_t block_index = rec_no / inodes_per_block;
    uint32_t block_offset = rec_no % inodes_per_block;

    // find the block where this inode is stored in
    uint32_t disk_block_no = 0;
    err = find_block_no_from_file_block_index(mt, db, block_index, &disk_block_no);
    if (err != OK) return err;

    // now we know the absolute block, we can read it
    err = mt->cache->read(mt->cache, disk_block_no, block_offset, &inmem->inode, sizeof(inode));
    if (err != OK) return err;

    inmem->flags = 0; // not dirty (so far)
    inmem->inode_db_rec_no = rec_no;
    return OK;
}

static int write_inode_to_inodes_database(mounted_data *mt, int rec_no, inode_in_mem *inmem) {
    int err;

    inode *db = &mt->superblock->inodes_db_inode;
    uint32_t inodes_in_file = db->file_size / sizeof(inode);
    if (rec_no >= inodes_in_file)
        return ERR_OUT_OF_BOUNDS;

    uint32_t inodes_per_block = mt->superblock->block_size_in_bytes / sizeof(inode);
    uint32_t block_index = rec_no / inodes_per_block;
    uint32_t block_offset = rec_no % inodes_per_block;

    // find the block where this inode is stored in
    uint32_t disk_block_no = 0;
    err = find_block_no_from_file_block_index(mt, db, block_index, &disk_block_no);
    if (err != OK) return err;

    // now we know the absolute block, we can write it
    err = mt->cache->write(mt->cache, disk_block_no, block_offset, &inmem->inode, sizeof(inode));
    if (err != OK) return err;

    inmem->flags = 0; // not dirty (any more)
    inmem->inode_db_rec_no = rec_no;
    return OK;
}

static int append_inode_to_inodes_database(mounted_data *mt, inode_in_mem *inmem, int *rec_no) {
    int err;

    // see if we need to add a block or can write in the last block.
    inode *db = &mt->superblock->inodes_db_inode;
    *rec_no = mt->superblock->max_inode_rec_no_in_db;
    uint32_t inodes_per_block = mt->superblock->block_size_in_bytes / sizeof(inode);
    uint32_t block_index = *rec_no / inodes_per_block;

    // add a block if needed
    if (block_index >= db->allocated_blocks) {
        uint32_t block_no;
        err = add_data_block_to_file(mt, db, &block_no);
        if (err != OK) return err;
    }

    err = write_inode_to_inodes_database(mt, *rec_no, inmem);
    if (err != OK) return err;

    // only when we are certain we passed.
    mt->superblock->max_inode_rec_no_in_db += 1;
    return OK;
}

static int delete_inode_from_inodes_database(mounted_data *mt, int rec_no) {
    inode_in_mem inmem;
    int err;

    err = read_inode_from_inodes_database(mt, rec_no, &inmem);
    if (err != OK) return err;
    memset(&inmem.inode, 0, sizeof(inode));
    err = write_inode_to_inodes_database(mt, rec_no, &inmem);
    if (err != OK) return err;

    return OK;
}

