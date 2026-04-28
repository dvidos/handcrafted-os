#include "sfs_internal.h"
#include "../../../logger/logger.h"

MODULE("SFS", LOG_LEVEL_DEBUG);



void sfs_stored_inode_log_debug(const char *prefix, stored_inode *n) {
    char buffer[64] = {0};

    char type = '?';
    if ((n->type_perms & STORED_INODE_TYPE_FILE)  == STORED_INODE_TYPE_FILE)  type = 'f';
    if ((n->type_perms & STORED_INODE_TYPE_DIR)   == STORED_INODE_TYPE_DIR)   type = 'd';
    if ((n->type_perms & STORED_INODE_TYPE_CHAR)  == STORED_INODE_TYPE_CHAR)  type = 'c';
    if ((n->type_perms & STORED_INODE_TYPE_BLOCK) == STORED_INODE_TYPE_BLOCK) type = 'b';
    if ((n->type_perms & STORED_INODE_TYPE_SYM)   == STORED_INODE_TYPE_SYM)   type = 's';

    log_debug("%s stored_inode(size=%lu, blocks=%lu, type/perms=0x%x (type=%c, perms=%c%c%c|%c%c%c|%c%c%c), created=%lu, modified=%lu, user=%d, group=%d, ranges=[%lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu], indirect=%lu, dbl indirect=%lu)",

        prefix,
        n->file_size, n->allocated_blocks,

        n->type_perms, 
        type,
        n->type_perms & STORED_INODE_PERM_USER_R   ? 'r' : '-',
        n->type_perms & STORED_INODE_PERM_USER_W   ? 'w' : '-',
        n->type_perms & STORED_INODE_PERM_USER_X   ? 'x' : '-',
        n->type_perms & STORED_INODE_PERM_GROUP_R  ? 'r' : '-',
        n->type_perms & STORED_INODE_PERM_GROUP_W  ? 'w' : '-',
        n->type_perms & STORED_INODE_PERM_GROUP_X  ? 'x' : '-',
        n->type_perms & STORED_INODE_PERM_OTHERS_R ? 'r' : '-',
        n->type_perms & STORED_INODE_PERM_OTHERS_W ? 'w' : '-',
        n->type_perms & STORED_INODE_PERM_OTHERS_X ? 'x' : '-',

        n->created_at, n->modified_at, n->user_id, n->user_id,
        
        n->ranges[0].first_block_no, n->ranges[0].blocks_count,
        n->ranges[1].first_block_no, n->ranges[1].blocks_count,
        n->ranges[2].first_block_no, n->ranges[2].blocks_count,
        n->ranges[3].first_block_no, n->ranges[3].blocks_count,
        n->indirect_ranges_block_no,
        n->double_indirect_block_no
    );
}


error_t sfs_inodes_db_dump_log(sfs_mount_data *mt, uint32_t first_inode_no, uint32_t count) {
    stored_inode *inodes_db;
    error_t err = mt->inode_cache->ops->get(mt->inode_cache, INODE_DB_INODE_ID, (void **)&inodes_db);
    if (err) return err;

    uint32_t records = (uint32_t)(inodes_db->file_size / sizeof(stored_inode));
    stored_inode si;
    char prefix[16];

    for (unsigned i = 0; i < count; i++) {
        ssize_t bytes = sfs_node_read_file_rec(mt, inodes_db, sizeof(stored_inode), first_inode_no + i, &si);
        if (bytes < 0) return (error_t) bytes;
        if (bytes != sizeof(stored_inode)) return ERR_CORRUPTION_DETECTED;

        sprintfn(prefix, sizeof(prefix), "inodes_db[%u]", first_inode_no + i);
        sfs_stored_inode_log_debug(prefix, &si);
    }
    return OK;
}


