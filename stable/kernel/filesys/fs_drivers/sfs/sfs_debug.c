#include "sfs_internal.h"



void sfs_stored_inode_formatter(log_write_stream_t *stream, va_list args) {
    stored_inode *n = va_arg(args, stored_inode *);
    char buffer[64] = {0};

    // stored_inode(size=0, blocks=0, type/perms=0x0 (----------), created=0, modified=0, user=0, group=0, ranges=[0:0, 0:0, 0:0, 0:0], ind_rng=0, dbl_ind_rng=0)

    char type = '-';
    if ((n->type_perms & STORED_INODE_TYPE_FILE)  == STORED_INODE_TYPE_FILE)  type = 'f';
    if ((n->type_perms & STORED_INODE_TYPE_DIR)   == STORED_INODE_TYPE_DIR)   type = 'd';
    if ((n->type_perms & STORED_INODE_TYPE_CHAR)  == STORED_INODE_TYPE_CHAR)  type = 'c';
    if ((n->type_perms & STORED_INODE_TYPE_BLOCK) == STORED_INODE_TYPE_BLOCK) type = 'b';
    if ((n->type_perms & STORED_INODE_TYPE_SYM)   == STORED_INODE_TYPE_SYM)   type = 's';

    stream->printf(stream, "(sz=%lu, blk=%lu, type/perms=0x%x (%c%c%c%c%c%c%c%c%c%c), ct=%lu, mt=%lu, u=%d, g=%d, rng=[%lu:%lu, %lu:%lu, %lu:%lu, %lu:%lu], ind=%lu, dbl-ind=%lu)",
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

        n->created_at, n->modified_at,
        n->user_id, n->group_id,
        
        n->ranges[0].first_block_no, n->ranges[0].blocks_count,
        n->ranges[1].first_block_no, n->ranges[1].blocks_count,
        n->ranges[2].first_block_no, n->ranges[2].blocks_count,
        n->ranges[3].first_block_no, n->ranges[3].blocks_count,
        n->indirect_ranges_block_no,
        n->double_indirect_block_no
    );
}
