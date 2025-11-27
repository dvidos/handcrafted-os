#include "internal.h"
#include <string.h>


static int open_inodes_register(mounted_data *mt, inode *iptr, uint32_t inode_id, open_inode **open_inode_ptr) {
    // find if already open
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        if (mt->open_inodes[i].is_used && mt->open_inodes[i].inode_id == inode_id) {
            *open_inode_ptr = &mt->open_inodes[i];
            return OK;
        }
    }

    // not found, try to find an empty slot
    int index;
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        if (!mt->open_inodes[i].is_used) {
            index = i;
            break;
        }
    }
    if (index == -1)
        return ERR_RESOURCES_EXHAUSTED; // too many open files
    
    mt->open_inodes[index].is_used = 1;
    mt->open_inodes[index].ref_count = 0;
    memcpy(&mt->open_inodes[index].inode_in_mem, iptr, sizeof(inode));
    mt->open_inodes[index].inode_id = inode_id;
    mt->open_inodes[index].is_dirty = 0;

    *open_inode_ptr = &mt->open_inodes[index];
    return OK;
}

static int open_inodes_release(mounted_data *mt, open_inode *node) {
    if (node->is_dirty)
        open_inodes_flush_inode(mt, node);

    node->is_used = 0;
    return OK;
}

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

static int open_handles_register(mounted_data *mt, open_inode *node, open_handle **handle_ptr) {
    // find a free slot
    int index = -1;
    for (int i = 0; i < MAX_OPEN_HANDLES; i++) {
        if (!mt->open_handles[i].is_used) {
            index = i;
            break;
        }
    }
    if (index == -1)
        return ERR_RESOURCES_EXHAUSTED;

    mt->open_handles[index].is_used = 1;
    mt->open_handles[index].inode = node;
    mt->open_handles[index].file_position = 0;
    mt->open_handles[index].inode->ref_count += 1; // one more references
    
    *handle_ptr = &mt->open_handles[index];
    return OK;
}

static int open_handles_release(mounted_data *mt, open_handle *handle) {
    handle->inode->ref_count -= 1;

    if (handle->inode->ref_count == 0) {
        int err = open_inodes_release(mt, handle->inode);
        if (err != OK) return err;
    }

    handle->is_used = 0;
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
