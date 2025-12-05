#include "internal.h"


#define INODE_CACHE_PINNED_ENTRIES   2  // for the inode db and the root dir


static int icache_get(mounted_data *mt, int inode_id, cached_inode **ptr) {
    cached_inode *cached = NULL;
    int err;

    // see if inode already in cache
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        cached = &mt->cached_inodes[i];
        if (cached->is_used && cached->inode_id == inode_id) {
            if (ptr != NULL)
                *ptr = cached;
            return OK;
        }
    }

    // we'll need to load - find a free slot
    cached = NULL;
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        if (!mt->cached_inodes[i].is_used) {
            cached = &mt->cached_inodes[i];
            break;
        }
    }

    // if we did not find a free slot, then evict something
    if (cached == NULL) {
        int slot = at_least(INODE_CACHE_PINNED_ENTRIES, mt->cached_inode_next_eviction); // first two slots not recycled
        cached = &mt->cached_inodes[slot];
        if (cached->is_dirty) {
            err = inode_db_update(mt, cached->inode_id, &cached->inode);
            if (err != OK) return err;
        }
        mt->cached_inode_next_eviction = (slot + 1) % MAX_OPEN_INODES;
    }

    // now we can load
    memset(cached, 0, sizeof(cached_inode));
    err = inode_db_load(mt, inode_id, &cached->inode);
    if (err != OK) return err;
    cached->inode_id = inode_id;
    cached->is_used = 1;
    cached->is_dirty = 0;
    cached->ref_count = 0;

    if (ptr != NULL)
        *ptr = cached;
    return OK;
}

// for usage after inode deletion
static int icache_invalidate_inode(mounted_data *mt, int inode_id) {
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        cached_inode *cached = &mt->cached_inodes[i];
        if (!cached->is_used) continue;
        if (cached->inode_id != inode_id) continue;

        // found it - invalidate it without saving
        cached->is_used = 0;
    }

    return OK; // it's not a failure to not find it.
}

static int icache_flush_all(mounted_data *mt) {
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        cached_inode *cached = &mt->cached_inodes[i];
        if (!cached->is_used) continue;
        if (!cached->is_dirty) continue;

        int err = inode_db_update(mt, cached->inode_id, &cached->inode);
        if (err != OK) return err;

        cached->is_dirty = 0;
    }

    return OK;
}

static int icache_is_inode_cached(mounted_data *mt, int inode_id) { // for debugging purposes
    for (int i = 0; i < MAX_OPEN_INODES; i++)
        if (mt->cached_inodes[i].is_used && mt->cached_inodes[i].inode_id == inode_id)
            return 1;
    return 0;
}

static void icache_dump_debug_info(mounted_data *mt) {
    char node_name[32];

    int used_inodes = 0;
    for (int i = 0; i < MAX_OPEN_INODES; i++)
        if (mt->cached_inodes[i].is_used)
            used_inodes++;
    
    printf("inodes cache (%d total slots, %d used)\n", MAX_OPEN_INODES, used_inodes);
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        cached_inode *n = &mt->cached_inodes[i];
        if (!n->is_used)
            continue;

        if (n->inode_id == INODE_DB_INODE_ID)
            strcpy(node_name, "INODES_DB");
        else if (n->inode_id == ROOT_DIR_INODE_ID)
            strcpy(node_name, "ROOT_DIR");
        else
            sprintf(node_name, "%u", n->inode_id);
        
        printf("    [%d]  id:%-10s  dirty:%d  refs:%u -> ", i, node_name, n->is_dirty, n->ref_count);
        inode_dump_debug_info(mt, "", &n->inode);
    }
}

