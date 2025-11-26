#include "internal.h"

// ----------------------------------------------------------------------------------------

static int inode_read_file_data(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length) {
    int err;

    if (file_pos >= n->file_size)
        return ERR_END_OF_FILE;

    uint32_t block_index = file_pos / mt->superblock->block_size_in_bytes;
    uint32_t block_offset = file_pos % mt->superblock->block_size_in_bytes;
    uint32_t disk_block_no = 0;
    int bytes_read = 0;

    // now we know the absolute block, we can read it
    while (length > 0) {
        err = find_block_no_from_file_block_index(mt, n, block_index, &disk_block_no);
        if (err != OK) return err;
        
        uint32_t max_length = mt->superblock->block_size_in_bytes - block_offset;
        uint32_t chunk_length = length > max_length ? max_length : length;
        err = cached_read(mt->cache, disk_block_no, block_offset, data, chunk_length);
        if (err != OK) return err;

        // prepare for next block
        block_index += 1;
        block_offset = 0;
        data += chunk_length;
        length -= chunk_length;
        bytes_read += chunk_length;
    }

    return bytes_read;
}

static int inode_write_file_data(mounted_data *mt, inode *n, uint32_t file_pos, void *data, uint32_t length) {
    int err;

    if (file_pos > n->file_size)
        file_pos = n->file_size;

    uint32_t block_index = file_pos / mt->superblock->block_size_in_bytes;
    uint32_t block_offset = file_pos % mt->superblock->block_size_in_bytes;
    uint32_t disk_block_no = 0;
    int bytes_written = 0;
    int at_eof = 0;

    // now we know the absolute block, we can read it
    while (length > 0) {
        // there may be space enough in the allocated blocks or we need to add a block
        if (block_index >= n->allocated_blocks) {
            err = add_data_block_to_file(mt, n, &disk_block_no);
            if (err != OK) return err;
            at_eof = 1;
        } else {
            err = find_block_no_from_file_block_index(mt, n, block_index, &disk_block_no);
            if (err != OK) return err;
        }

        // write this chunk
        uint32_t max_length = mt->superblock->block_size_in_bytes - block_offset;
        uint32_t chunk_length = length > max_length ? max_length : length;
        err = cached_write(mt->cache, disk_block_no, block_offset, data, chunk_length);
        if (err != OK) return err;

        // prepare for next block
        block_index += 1;
        block_offset = 0;
        data += chunk_length;
        length -= chunk_length;
        bytes_written += chunk_length;

        if (at_eof)
            n->file_size += chunk_length;
    }

    return bytes_written;
}

// -----------------------------------------------------------------------------

static int inode_load(mounted_data *mt, uint32_t inode_rec_no, inode *node) {
    int bytes = inode_read_file_data(mt, &mt->superblock->inodes_db_inode,
        inode_rec_no * sizeof(inode), 
        node,
        sizeof(inode)
    );
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    return OK;
}

static int inode_allocate(mounted_data *mt, int is_dir, inode *node, uint32_t *inode_rec_no) {
    memset(node, 0, sizeof(inode));
    if (is_dir) {
        node->is_dir = 1;
    } else {
        node->is_file = 1;
    }
    node->is_used = 1;

    // this should have a lock somehow
    *inode_rec_no = mt->superblock->inodes_db_rec_count;
    int bytes = inode_write_file_data(mt, node, *inode_rec_no * sizeof(inode), node, sizeof(inode));
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    mt->superblock->inodes_db_rec_count += 1;
    return OK;
}

static int inode_persist(mounted_data *mt, uint32_t inode_rec_no, inode *node) {
    if (inode_rec_no == ROOT_DIR_INODE_REC_NO) {
        memcpy(&mt->superblock->root_dir_inode, node, sizeof(inode));
        return OK;
    }

    int bytes = inode_write_file_data(mt, &mt->superblock->inodes_db_inode, 
        inode_rec_no * sizeof(inode),
        node,
        sizeof(inode)
    );
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    return OK;
}

static int inode_delete(mounted_data *mt, uint32_t inode_rec_no) {
    if (inode_rec_no == ROOT_DIR_INODE_REC_NO)
        return ERR_NOT_PERMITTED;
    
    inode blank;
    memset(&blank, 0, sizeof(inode));

    int bytes = inode_write_file_data(mt, &mt->superblock->inodes_db_inode,
        inode_rec_no * sizeof(inode),
        &blank,
        sizeof(inode)
    );
    if (bytes < 0) return bytes;
    if (bytes < sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;

    return OK;
}

