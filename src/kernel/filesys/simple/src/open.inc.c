#include "internal.h"
#include <string.h>


static int open_inodes_flush_inode(mounted_data *mt, open_inode *node) {
    if (node->inode_id == ROOT_DIR_INODE_ID) {
        // save in superblock, in memory
        memcpy(&mt->superblock->root_dir_inode, &node->inode_in_mem, sizeof(inode));
    } else {
        // save in inodes database
        int bytes = inode_write_file_bytes(mt, &mt->superblock->inodes_db_inode, node->inode_id * sizeof(inode), &node->inode_in_mem, sizeof(inode));
        if (bytes < 0) return bytes;
        if (bytes != sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;
    }

    node->is_dirty = 0;
    return OK;
}

static int open_handles_release(mounted_data *mt, open_handle *handle) {
    handle->inode->ref_count -= 1;

    if (handle->inode->ref_count == 0) {
        if (handle->inode->is_dirty) {
            int err = open_inodes_flush_inode(mt, handle->inode);
            if (err != OK) return err;
        }

        handle->inode->is_used = 0;
    }

    handle->is_used = 0;
    return OK;
}

static open_inode *open_files_find_specific_inode_slot(mounted_data *mt, uint32_t inode_id) {
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        if (mt->open_inodes[i].is_used && mt->open_inodes[i].inode_id == inode_id)
            return &mt->open_inodes[i];
    }
    return NULL;
}

static open_inode *open_files_find_unused_inode_slot(mounted_data *mt) {
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        if (!mt->open_inodes[i].is_used)
            return &mt->open_inodes[i];
    }
    return NULL;
}

static open_handle *open_files_find_unused_handle_slot(mounted_data *mt) {
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (!mt->open_handles[i].is_used)
            return &mt->open_handles[i];
    }
    return NULL;
}

static int open_files_register(mounted_data *mt, inode *node, uint32_t inode_id, open_handle **handle_ptr) {
    open_inode *onode;
    open_handle *ohandle;

    // find if inode is already open
    onode = open_files_find_specific_inode_slot(mt, inode_id);
    if (onode == NULL) {
        // find slot top open it on
        onode = open_files_find_unused_inode_slot(mt);
        if (onode == NULL) return ERR_RESOURCES_EXHAUSTED;

        // reset this recycled slot
        onode->is_used = 1;
        onode->ref_count = 0;
        memcpy(&onode->inode_in_mem, node, sizeof(inode));
        onode->inode_id = inode_id;
        onode->is_dirty = 0;
    }
    
    // now find a slot top open the handle on
    ohandle = open_files_find_unused_handle_slot(mt);
    if (ohandle == NULL) {
        // release open_inode as well, if not used
        if (onode->ref_count == 0) 
            onode->is_used = 0;
        return ERR_RESOURCES_EXHAUSTED;
    }

    // initialize this handle
    ohandle->is_used = 1;
    ohandle->file_position = 0;
    ohandle->inode = onode;
    ohandle->inode->ref_count += 1; // one more references
    
    *handle_ptr = ohandle;
    return OK;
}

static int open_files_flush_dirty_inodes(mounted_data *mt) {
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        open_inode *node = &mt->open_inodes[i];
        if (!node->is_used || !node->is_dirty)
            continue;
        
        if (node->inode_id == ROOT_DIR_INODE_ID) {
            // save in superblock, in memory
            memcpy(&mt->superblock->root_dir_inode, &node->inode_in_mem, sizeof(inode));
        } else {
            // save in inodes database
            int bytes = inode_write_file_bytes(mt, &mt->superblock->inodes_db_inode, node->inode_id * sizeof(inode), &node->inode_in_mem, sizeof(inode));
            if (bytes < 0) return bytes;
            if (bytes != sizeof(inode)) return ERR_RESOURCES_EXHAUSTED;
        }
        node->is_dirty = 0;
    }

    return OK;
}

static void open_dump_debug_info(mounted_data *mt) {
    int used_inodes = 0;
    int used_handles = 0;
    for (int i = 0; i < MAX_OPEN_INODES; i++)
        if (mt->open_inodes[i].is_used)
            used_inodes++;
    for (int i = 0; i < MAX_OPEN_HANDLES; i++)
        if (mt->open_handles[i].is_used)
            used_handles++;
    
    printf("Open inodes (%d total, %d used)\n", MAX_OPEN_INODES, used_inodes);
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        open_inode *n = &mt->open_inodes[i];
        if (!n->is_used)
            continue;;
        printf("    [%d]  dirty:%d, inode:%u, refs:%u -> ", i, n->is_dirty, n->inode_id, n->ref_count);
        inode_dump_debug_info("", &n->inode_in_mem);
    }
    printf("Open file handles (%d total, %d used)\n", MAX_OPEN_HANDLES, used_handles);
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        open_handle *h = &mt->open_handles[i];
        if (!h->is_used)
            continue;
        printf("    [%d]  inode:%u, fpos:%u\n", i, h->inode->inode_id, h->file_position);
    }
}
