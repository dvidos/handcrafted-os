#include "sfs_internal.h"
#include "../../../klib/string.h"
#include "../../../drivers/clock.h"



ssize_t sfs_read_file_data(sfs_mount_data *mt, stored_inode *sin, uint64_t file_pos, void *data, size_t length) {
    log_trace("sfs_read_file_data(nod=%p, pos=%llu, len=%lu)", sin, file_pos, length);
    if (file_pos >= sin->file_size)
        return 0; // EOF

    size_t bytes_to_read = MIN(length, sin->file_size - file_pos);
    if (bytes_to_read == 0)
        return 0;

    error_t err = OK;
    uint32_t block_size = mt->superblock->block_size_in_bytes;

    // where to start reading
    uint32_t block_index = file_pos / block_size;
    uint32_t offset_in_block = file_pos % block_size;
    
    // work in chunks
    ssize_t total_bytes_read = 0;
    char *out_ptr = (char *)data;
    while (bytes_to_read > 0) {
        size_t chunk_size = MIN(bytes_to_read, block_size - offset_in_block);
        log_debug("read loop: bytes_to_read=%d, block_index=%d, block_offset=%d, chunk_size=%d", bytes_to_read, block_index, offset_in_block, chunk_size);
        
        block_no_t data_block_no = 0;
        err = sfs_node_resolve_data_block(mt, sin, block_index, &data_block_no);
        if (err) return traceable(err);
        
        err = sfs_cached_read(mt, data_block_no, offset_in_block, out_ptr, chunk_size);
        if (err) return traceable(err);

        out_ptr += chunk_size;
        total_bytes_read += chunk_size;
        bytes_to_read -= chunk_size;
        
        block_index++;
        offset_in_block = 0; // next reads will be from the start of the block
    }

    return total_bytes_read;
}

ssize_t sfs_write_file_data(sfs_mount_data *mt, stored_inode *sin, uint64_t file_pos, const void *data, size_t length) {
    log_trace("sfs_write_file_data(nod=%p, pos=%llu, len=%lu)", sin, file_pos, length);
    if (mt->is_readonly)
        return traceable(ERR_NOT_PERMITTED);

    error_t err = OK;
    uint32_t block_size = mt->superblock->block_size_in_bytes;

    // expand file if needed
    uint64_t end_pos = file_pos + length;
    uint32_t blocks_needed = (uint32_t)((end_pos + block_size - 1) / block_size);
    while (sin->allocated_blocks < blocks_needed) {
        log_debug("allocate loop: blocks_allocated=%d, blocks_needed=%d", sin->allocated_blocks, blocks_needed);
        err = sfs_node_expand_data_blocks(mt, sin);
        if (err) return traceable(err);
        sin->allocated_blocks += 1;
    }
    log_debug_fmt(sfs_stored_inode_formatter, "after allocation", sin);
    
    // find where to start writing
    uint32_t block_index = file_pos / block_size;
    uint32_t offset_in_block = file_pos % block_size;
    size_t length_remaining = length;
    
    // work in chunks
    ssize_t total_bytes_written = 0;
    const char *in_ptr = (const char *)data;
    while (length_remaining > 0) {
        size_t chunk_size = MIN(length_remaining, block_size - offset_in_block);
        log_debug("write loop: length_remaining=%d, block_index=%d, block_offset=%d, chunk_size=%d", length_remaining, block_index, offset_in_block, chunk_size);

        block_no_t data_block_no = 0;
        err = sfs_node_resolve_data_block(mt, sin, block_index, &data_block_no);
        if (err) return traceable(err);
        
        err = sfs_cached_write(mt, data_block_no, offset_in_block, in_ptr, chunk_size);
        if (err) return traceable(err);

        in_ptr += chunk_size;
        total_bytes_written += chunk_size;
        length_remaining -= chunk_size;
        
        block_index++;
        offset_in_block = 0; // next writes will be from the start of the block
    }

    // adjust file size as needed
    if (file_pos + length > sin->file_size) {
        sin->file_size = file_pos + length;
    }
    // sin->modified_at = time(NULL);
    sin->modified_at = get_seconds_since_1970();

    return total_bytes_written;
}

// ----------------------------------------------------------------

error_t sfs_load_direntry(sfs_mount_data *mt, stored_inode *dir_inode, size_t entry_no, stored_dir_entry *entry) {
    log_trace("sfs_load_direntry(entry_no=%lu, entry=%p)", entry_no, entry);
    // we use the block cache, and avoid the inode cache complexity
    // but really, using 0.39% of the disk size (one inode every 16K of data) in pre-calculated positions is really faster
    // resolving the data block is the slowness... maybe if all data blocks of the inode db are cached, we can be fast...
    ssize_t bytes = sfs_read_file_data(mt, dir_inode,
        entry_no * sizeof(stored_dir_entry),
        entry, 
        sizeof(stored_dir_entry)
    );
    if (bytes < 0) return traceable((error_t)bytes);
    if ((size_t)bytes < sizeof(stored_dir_entry)) return ERR_READING_FILE;
    // don't forget to update and store the inode
    return OK;
}

error_t sfs_save_direntry(sfs_mount_data *mt, stored_inode *dir_inode, size_t entry_no, stored_dir_entry *entry) {
    log_trace("sfs_save_direntry(entry_no=%lu, entry=%p)", entry_no, entry);
    // we use the block cache, and avoid the inode cache complexity
    // but really, using 0.39% of the disk size (one inode every 16K of data) in pre-calculated positions is really faster
    // resolving the data block is the slowness... maybe if all data blocks of the inode db are cached, we can be fast...
    ssize_t bytes = sfs_write_file_data(mt, dir_inode,
        entry_no * sizeof(stored_dir_entry),
        entry, 
        sizeof(stored_dir_entry)
    );
    if (bytes < 0) return traceable((error_t)bytes);
    if ((size_t)bytes < sizeof(stored_dir_entry)) return ERR_WRITING_FILE;
    // don't forget to update and store the inode
    return OK;
}

error_t sfs_find_direntry(sfs_mount_data *mt, stored_inode *dir_inode, const char *name, size_t *entry_no, inode_no_t *target_inode_no) {
    stored_dir_entry entry;

    size_t records = dir_inode->file_size / sizeof(stored_dir_entry);
    for (size_t i = 0; i < records; i++) {
        error_t err = sfs_load_direntry(mt, dir_inode, i, &entry);
        if (err) return traceable(err);

        if (strcmp(entry.name, name) == 0) {
            *entry_no = i;
            *target_inode_no = entry.inode_num;
            return OK;
        }
    }

    return ERR_NOT_FOUND;
}

error_t sfs_clear_direntry(sfs_mount_data *mt, stored_inode *dir_inode, size_t entry_no) {
    stored_dir_entry entry;

    memset(&entry, 0, sizeof(stored_dir_entry));
    return sfs_save_direntry(mt, dir_inode, entry_no, &entry);
}

error_t sfs_add_direntry(sfs_mount_data *mt, stored_inode *dir_inode, const char *name, inode_no_t target_inode_no, size_t *entry_no) {
    // save at the end or in an empty entry.
    stored_dir_entry entry;
    error_t err;

    // ideally, we should make sure that the cropped version of the file does not already exist...
    if (strlen(name) > MAX_FILENAME_LENGTH)
        return ERR_NAME_TOO_LONG;
    
    size_t records = dir_inode->file_size / sizeof(stored_dir_entry);
    for (size_t i = 0; i < records; i++) {
        err = sfs_load_direntry(mt, dir_inode, i, &entry);
        if (err) return traceable(err);

        if (!stored_dir_entry_is_used(&entry)) {
            strscpy(entry.name, name, MAX_FILENAME_LENGTH);
            entry.inode_num = target_inode_no;
            err = sfs_save_direntry(mt, dir_inode, i, &entry);
            *entry_no = i;
            return traceable(err);
        }
    }

    // we did not find any empty entry to re-use, append it
    strscpy(entry.name, name, MAX_FILENAME_LENGTH);
    entry.inode_num = target_inode_no;
    err = sfs_save_direntry(mt, dir_inode, records, &entry);
    *entry_no = records;
    return traceable(err);
}

error_t sfs_check_dir_emptiness(sfs_mount_data *mt, stored_inode *dir_inode, bool *is_empty) {
    // check if entries
    stored_dir_entry entry;
    error_t err;

    size_t records = dir_inode->file_size / sizeof(stored_dir_entry);
    for (size_t i = 0; i < records; i++) {
        err = sfs_load_direntry(mt, dir_inode, i, &entry);
        if (err) return traceable(err);

        if (entry.name[0] == 0 || strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0)
            continue;

        // there is something we cannot ignore
        *is_empty = false;
        return OK;

    }

    *is_empty = true;
    return OK;
}
