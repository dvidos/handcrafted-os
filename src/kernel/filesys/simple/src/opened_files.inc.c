#include "internal.h"
#include <string.h>


static int opened_inode_flush(mounted_data *mt, cached_inode *node) {
    if (node->inode_id == INODE_DB_INODE_ID) {
        // save in superblock, in memory
        memcpy(&mt->superblock->inodes_db_inode, &node->inode_in_mem, sizeof(saved_inode));

    } else if (node->inode_id == ROOT_DIR_INODE_ID) {
        // save in superblock, in memory
        memcpy(&mt->superblock->root_dir_inode, &node->inode_in_mem, sizeof(saved_inode));

    } else {
        // TODO: improve the speed of this
        cached_inode *inodes_db;
        int err = get_cached_inode(mt, INODE_DB_INODE_ID, &inodes_db);
        if (err != OK) return err;

        // save in inodes database
        int bytes = inode_write_file_bytes(mt, inodes_db, node->inode_id * sizeof(saved_inode), &node->inode_in_mem, sizeof(saved_inode));
        if (bytes < 0) return bytes;
        if (bytes != sizeof(saved_inode)) return ERR_RESOURCES_EXHAUSTED;
    }

    node->is_dirty = 0;
    return OK;
}

static open_handle *opened_files_find_unused_handle_slot(mounted_data *mt) {
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (!mt->open_handles[i].is_used)
            return &mt->open_handles[i];
    }
    return NULL;
}

static int opened_files_register(mounted_data *mt, cached_inode *cinode, open_handle **handle_ptr) {

    // now find a slot top open the handle on
    open_handle *ohandle = opened_files_find_unused_handle_slot(mt);
    if (ohandle == NULL) return ERR_RESOURCES_EXHAUSTED;

    // initialize this handle
    ohandle->is_used = 1;
    ohandle->file_position = 0;
    ohandle->inode = cinode;
    ohandle->inode->ref_count += 1; // one more references

    *handle_ptr = ohandle;
    return OK;
}

static int opened_handles_release(mounted_data *mt, open_handle *handle) {

    // close inode as well, if no other references
    handle->inode->ref_count -= 1;
    if (handle->inode->ref_count == 0) {
        if (handle->inode->is_dirty) {
            int err = opened_inode_flush(mt, handle->inode);
            if (err != OK) return err;
        }
        handle->inode->is_used = 0;
    }

    // close the handle now
    handle->is_used = 0;
    return OK;
}

static void opened_files_dump_debug_info(mounted_data *mt) {
    int used_handles = 0;
    for (int i = 0; i < MAX_OPEN_HANDLES; i++)
        if (mt->open_handles[i].is_used)
            used_handles++;
    
    printf("Open file handles (%d total, %d used)\n", MAX_OPEN_HANDLES, used_handles);
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        open_handle *h = &mt->open_handles[i];
        if (!h->is_used)
            continue;
        printf("    [%d]  inode:%u, fpos:%u\n", i, h->inode->inode_id, h->file_position);
    }
}
