#include "internal.h"


static int inode_db_rec_count(mounted_data *mt) {
    return mt->cached_inodes_db_inode->inode.file_size / sizeof(stored_inode);
}

static int inode_db_load(mounted_data *mt, uint32_t inode_id, stored_inode *node) {
    if (inode_id == INODE_DB_INODE_ID) {
        memcpy(node, &mt->superblock->inodes_db_inode, sizeof(stored_inode));
        return OK;
    } else if (inode_id == ROOT_DIR_INODE_ID) {
        memcpy(node, &mt->superblock->root_dir_inode, sizeof(stored_inode));
        return OK;
    }

    int err = inode_read_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), inode_id, node);
    if (err != OK) return err;

    return OK;
}

static stored_inode inode_prepare(clock_device *clock, int for_dir) {
    stored_inode node;

    memset(&node, 0, sizeof(stored_inode));
    if (for_dir)
        node.is_dir = 1;
    else
        node.is_file = 1;
    node.is_used = 1;
    node.modified_at = clock->get_seconds_since_epoch(clock);

    return node;
}

static int inode_db_append(mounted_data *mt, stored_inode *node, uint32_t *inode_id) {
    int recs = inode_db_rec_count(mt);
    if (recs >= UINT32_MAX - 2)
        return ERR_RESOURCES_EXHAUSTED; // try reusing deleted inodes?

    // this should have a lock somehow (race conditions)
    int rec_no = recs;
    int err = inode_write_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), rec_no, node);
    if (err != OK) return err;

    *inode_id = rec_no;
    mt->superblock->inodes_db_rec_count = recs + 1;
    return OK;
}

static int inode_db_update(mounted_data *mt, uint32_t inode_id, stored_inode *node) {
    if (inode_id == INODE_DB_INODE_ID) {
        memcpy(&mt->superblock->inodes_db_inode, node, sizeof(stored_inode));
        return OK;
    } else if (inode_id == ROOT_DIR_INODE_ID) {
        memcpy(&mt->superblock->root_dir_inode, node, sizeof(stored_inode));
        return OK;
    }

    int err = inode_write_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), inode_id, node);
    if (err != OK) return err;

    return OK;
}

static int inode_db_delete(mounted_data *mt, uint32_t inode_id) {
    if (inode_id == INODE_DB_INODE_ID || inode_id == ROOT_DIR_INODE_ID)
        return ERR_NOT_PERMITTED;
    
    int recs = inode_db_rec_count(mt);
    if (inode_id == recs - 1) {
        // shorten the file, but keep blocks for simplicity
        mt->cached_inodes_db_inode->inode.file_size = (recs - 1) * sizeof(stored_inode);
        mt->superblock->inodes_db_rec_count = recs - 1;

    } else {
        stored_inode blank;
        memset(&blank, 0, sizeof(stored_inode));
        int err = inode_write_file_rec(mt, mt->cached_inodes_db_inode, sizeof(stored_inode), inode_id, &blank);
        if (err != OK) return err;
    }

    return OK;
}

