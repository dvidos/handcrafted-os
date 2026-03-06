#include "sfs_internal.h"
#include "../../../klib/string.h"
#include "../../../drivers/clock.h"
#include "../../../utils/logger.h"

MODULE("SFS_NRW", LOG_LEVEL_WARN);


ssize_t sfs_node_read_file_bytes(sfs_mount_data *mt, stored_inode *sin, uint64_t file_pos, void *data, size_t length) {
    log_trace("sfs_node_read_file_bytes(nod=%p, pos=%llu, len=%lu)", sin, file_pos, length);
    if (file_pos >= sin->file_size)
        return 0; // EOF

    size_t bytes_to_read = MIN(length, sin->file_size - file_pos);
    if (bytes_to_read == 0)
        return 0;

    uint32_t block_size = mt->superblock->block_size_in_bytes;
    uint32_t block_index = file_pos / block_size;
    uint32_t offset_in_block = file_pos % block_size;
    
    ssize_t total_bytes_read = 0;
    char *out_ptr = (char *)data;

    while (bytes_to_read > 0) {
        block_no_t data_block_no;
        error_t err = sfs_node_resolve_data_block(mt, sin, block_index, &data_block_no);
        if (err) return err;

        err = sfs_block_read(mt, data_block_no, mt->generic_block_buffer);
        if (err) return err;

        size_t chunk_size = MIN(bytes_to_read, block_size - offset_in_block);
        memcpy(out_ptr, mt->generic_block_buffer + offset_in_block, chunk_size);
        
        out_ptr += chunk_size;
        total_bytes_read += chunk_size;
        bytes_to_read -= chunk_size;
        
        block_index++;
        offset_in_block = 0; // next reads will be from the start of the block
    }

    return total_bytes_read;
}

ssize_t sfs_node_write_file_bytes(sfs_mount_data *mt, stored_inode *sin, inode_no_t inode_num, uint64_t file_pos, const void *data, size_t length) {
    log_trace("sfs_node_write_file_bytes(nod=%p, pos=%llu, len=%lu)", sin, file_pos, length);
    if (mt->is_readonly)
        return ERR_NOT_PERMITTED;

    error_t err;
    uint32_t block_size = mt->superblock->block_size_in_bytes;

    // expand file if needed
    uint64_t end_pos = file_pos + length;
    while (end_pos > sin->file_size) {
        uint32_t blocks_needed = (end_pos - sin->file_size + block_size - 1) / block_size;
        for (uint32_t i = 0; i < blocks_needed; i++) {
            err = sfs_node_expand_data_blocks(mt, sin, inode_num);
            if (err) return err;
            sin->file_size += block_size;
        }
    }
    
    uint32_t block_index = file_pos / block_size;
    uint32_t offset_in_block = file_pos % block_size;
    
    ssize_t total_bytes_written = 0;
    const char *in_ptr = (const char *)data;
    size_t bytes_to_write = length;

    while (bytes_to_write > 0) {
        block_no_t data_block_no;
        err = sfs_node_resolve_data_block(mt, sin, block_index, &data_block_no);
        if (err) return err;

        size_t chunk_size = MIN(bytes_to_write, block_size - offset_in_block);

        // if it's a partial write, we need to read the block first
        if (chunk_size < block_size) {
            err = sfs_block_read(mt, data_block_no, mt->generic_block_buffer);
            if (err) return err;
        }

        memcpy(mt->generic_block_buffer + offset_in_block, in_ptr, chunk_size);
        
        err = sfs_block_write(mt, data_block_no, mt->generic_block_buffer);
        if (err) return err;
        
        in_ptr += chunk_size;
        total_bytes_written += chunk_size;
        bytes_to_write -= chunk_size;
        
        block_index++;
        offset_in_block = 0; // next writes will be from the start of the block
    }

    if (file_pos + length > sin->file_size) {
        sin->file_size = file_pos + length;
    }
    // sin->modified_at = time(NULL);
    sin->modified_at = get_seconds_since_1970();
    mt->inode_cache->ops->mark_dirty(mt->inode_cache, inode_num);

    return total_bytes_written;
}

ssize_t sfs_node_read_file_rec(sfs_mount_data *mt, stored_inode *sin, size_t rec_size, uint32_t rec_no, void *rec) {
    log_trace("sfs_node_read_file_rec(nod=%p, rec=%lu, size=%lu)", sin, rec_no, rec_size);
    return sfs_node_read_file_bytes(mt, sin, rec_size * rec_no, rec, rec_size);
}

ssize_t sfs_node_write_file_rec(sfs_mount_data *mt, stored_inode *sin, inode_no_t inode_num, size_t rec_size, uint32_t rec_no, const void *rec) {
    log_trace("sfs_node_write_file_rec(nod=%p, rec=%lu, size=%lu)", sin, rec_no, rec_size);
    return sfs_node_write_file_bytes(mt, sin, inode_num, rec_size * rec_no, rec, rec_size);
}
