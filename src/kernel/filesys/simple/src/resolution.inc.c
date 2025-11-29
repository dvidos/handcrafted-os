#include "internal.h"



static int flexible_path_resolution(mounted_data *mt, const char *path, int resolve_parent_dir_only, inode *target, uint32_t *inode_id) {
    char part_name[MAX_FILENAME_LENGTH + 1];
    int err;
    inode curr_dir;
    uint32_t curr_dir_inode_id;
    inode entry_inode;
    uint32_t entry_inode_id;

    // all paths should be absolute
    if (path[0] != '/')
        return ERR_INVALID_ARGUMENT;
    path += 1;

    // path was "/" only.
    if (*path == '\0') {
        if (resolve_parent_dir_only)
            return ERR_INVALID_ARGUMENT;

        memcpy(target, &mt->superblock->root_dir_inode, sizeof(inode));
        *inode_id = ROOT_DIR_INODE_ID;
        return OK;
    }

    // start walking down the directories from root dir
    memcpy(&curr_dir, &mt->superblock->root_dir_inode, sizeof(inode));
    curr_dir_inode_id = ROOT_DIR_INODE_ID;
    while (1) {

        // get first part of the path
        path_get_first_part(path, part_name, sizeof(part_name));
        path += strlen(part_name);
        if (*path == '/')
            path++;
        int finished = (*path == '\0');

        // if we are resolving parent, we don't need to seek the last part
        if (resolve_parent_dir_only && finished) {
            memcpy(target, &curr_dir, sizeof(inode));
            *inode_id = curr_dir_inode_id;
            return OK;
        }

        // look up the chunk in the current directory
        err = dir_entry_find(mt, &curr_dir, part_name, &entry_inode_id, NULL);
        if (err != OK) return err;

        // load so we can return it, or use it.
        err = inode_load(mt, entry_inode_id, &entry_inode);
        if (err != OK) return err;

        if (finished) {
            memcpy(target, &entry_inode, sizeof(inode));
            *inode_id = entry_inode_id;
            return OK;
        }

        // verify we recurse into a directory that we can search in
        if (!entry_inode.is_dir)
            return ERR_WRONG_TYPE;
        memcpy(&curr_dir, &entry_inode, sizeof(inode));
        curr_dir_inode_id = entry_inode_id;
    }

    // should never happen
    return ERR_NOT_FOUND;
}


static int resolve_path_to_inode(mounted_data *mt, const char *path, inode *target, uint32_t *inode_id) {
    return flexible_path_resolution(mt, path, 0, target, inode_id);
}

static int resolve_path_parent_to_inode(mounted_data *mt, const char *path, inode *target, uint32_t *inode_id) {
    return flexible_path_resolution(mt, path, 1, target, inode_id);
}
