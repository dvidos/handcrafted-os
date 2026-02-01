#include "internal.h"



static int flexible_path_resolution(mounted_data *mt, const char *path, int resolve_parent_dir_only, cached_inode **cached_inode_ptr) {
    char part_name[MAX_FILENAME_LENGTH + 1];
    int err;
    
    cached_inode *cached_dir = NULL;
    cached_inode *cached_entry = NULL;

    // all paths should be absolute
    if (path[0] != '/')
        return ERR_INVALID_ARGUMENT;
    path += 1;

    // path was "/" only.
    if (*path == '\0') {
        if (resolve_parent_dir_only)
            return ERR_INVALID_ARGUMENT;

        return icache_get(mt, ROOT_DIR_INODE_ID, cached_inode_ptr);
    }

    // start walking down the directories from root dir
    err = icache_get(mt, ROOT_DIR_INODE_ID, &cached_dir);
    while (1) {

        // get first part of the path
        path_get_first_part(path, part_name, sizeof(part_name));
        path += strlen(part_name);
        if (*path == '/')
            path++;
        int path_finished = (*path == '\0');

        // if we are resolving parent, we don't need to seek the last part
        if (resolve_parent_dir_only && path_finished) {
            *cached_inode_ptr = cached_dir;
            return OK;
        }

        // look up the chunk in the current directory
        uint32_t entry_inode_id;
        err = dir_find_entry_by_name(mt, cached_dir, part_name, &entry_inode_id, NULL);
        if (err != OK) return err;

        // load so we can return it, or use it.
        err = icache_get(mt, entry_inode_id, &cached_entry);
        if (err != OK) return err;

        if (path_finished) {
            *cached_inode_ptr = cached_entry;
            return OK;
        }

        // otherwise, recurse into this directory -- ensure it's a directory
        if (!cached_entry->inode.is_dir)
            return ERR_WRONG_TYPE;
        cached_dir = cached_entry;
    }

    // should never happen
    return ERR_NOT_FOUND;
}

static int resolve_path_to_inode(mounted_data *mt, const char *path, cached_inode **cached_inode_ptr) {
    return flexible_path_resolution(mt, path, 0, cached_inode_ptr);
}

static int resolve_path_parent_to_inode(mounted_data *mt, const char *path, cached_inode **cached_inode_ptr) {
    return flexible_path_resolution(mt, path, 1, cached_inode_ptr);
}
