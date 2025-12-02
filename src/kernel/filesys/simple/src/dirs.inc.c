#include "internal.h"

#define DIR_ENTRY_MAX_REC_NO     0x7FFFFFFF

static int dir_entry_find(mounted_data *mt, cached_inode *dir_inode, const char *name, uint32_t *inode_id, uint32_t *entry_rec_no) {
    stored_dir_entry entry;

    for (int rec_no = 0; rec_no < DIR_ENTRY_MAX_REC_NO; rec_no++) {
        int bytes = inode_read_file_bytes(mt, dir_inode, rec_no * sizeof(stored_dir_entry), &entry, sizeof(stored_dir_entry));
        if (bytes == ERR_END_OF_FILE) break;
        if (bytes < 0) return bytes;

        if (strcmp(entry.name, name) == 0) {
            *inode_id = entry.inode_id;
            if (entry_rec_no != NULL)
                *entry_rec_no = rec_no;
            return OK;
        }
    }

    return ERR_NOT_FOUND;
}

static int dir_entry_update(mounted_data *mt, cached_inode *dir_inode, int entry_rec_no, const char *name, uint32_t inode_id) {
    stored_dir_entry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_id = inode_id;

    int bytes = inode_write_file_bytes(mt, dir_inode, entry_rec_no * sizeof(stored_dir_entry), &entry, sizeof(stored_dir_entry));
    if (bytes < 0) return bytes;

    return OK;
}

static int dir_entry_append(mounted_data *mt, cached_inode *dir_inode, const char *name, uint32_t inode_id) { 
    stored_dir_entry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_id = inode_id;

    int bytes = inode_write_file_bytes(mt, dir_inode, dir_inode->inode_in_mem.file_size, &entry, sizeof(stored_dir_entry));
    if (bytes < 0) return bytes;

    return OK;
}

static int dir_entry_ensure_missing(mounted_data *mt, cached_inode *dir_inode, const char *name) {
    stored_dir_entry entry;

    for (int rec_no = 0; rec_no < DIR_ENTRY_MAX_REC_NO; rec_no++) {
        int bytes = inode_read_file_bytes(mt, dir_inode, rec_no * sizeof(stored_dir_entry), &entry, sizeof(stored_dir_entry));
        if (bytes == ERR_END_OF_FILE) break;
        if (bytes < 0) return bytes;

        if (strcmp(entry.name, name) == 0)
            return ERR_ALREADY_EXISTS;
    }

    return OK;
}

static int dir_entry_delete(mounted_data *mt, cached_inode *dir_inode, int entry_rec_no) {
    int recs_in_file = dir_inode->inode_in_mem.file_size / sizeof(stored_dir_entry);
    if (entry_rec_no == recs_in_file - 1) {
        // shorten the file, but keep blocks for simplicity
        dir_inode->inode_in_mem.file_size = (recs_in_file - 1) * sizeof(stored_dir_entry);
        return OK;
    } else {
        return dir_entry_update(mt, dir_inode, entry_rec_no, "", 0); // remove in place
    }
}

static void dir_dump_debug_info(mounted_data *mt, cached_inode *dir_inode, int depth) {
    struct stored_dir_entry entry;
    int rec_no;
    int err, bytes;

    for (rec_no = 0; 0x7FFFFFFF; rec_no++) {
        bytes = inode_read_file_bytes(mt, dir_inode, rec_no * sizeof(stored_dir_entry), &entry, sizeof(stored_dir_entry));
        if (bytes < 0) break;
        int is_special = (strcmp(entry.name, ".") == 0) || (strcmp(entry.name, "..") == 0);

        printf("%*s%-16s  inode:%u  ", depth * 4, "", entry.name, entry.inode_id);
        if (is_special) {
            printf("\n");
        } else {
            // here, if we read from disk, the node may be already changed in the cache
            stored_inode child_node;
            if (is_inode_cached(mt, entry.inode_id)) {
                cached_inode *cached;
                err = get_cached_inode(mt, entry.inode_id, &cached);
                if (err != OK) break;
                memcpy(&child_node, &cached->inode_in_mem, sizeof(stored_inode));
            } else {
                err = inode_load(mt, entry.inode_id, &child_node);
                if (err != OK) break;
            }

            inode_dump_debug_info("", &child_node);
            // we cannod recurse into subdir without opening it, affecting the cache...
        }
    }
    if (rec_no == 0) {
        printf("%*s(no entries)\n", depth * 4, "");
    }
}


