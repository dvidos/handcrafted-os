#include "internal.h"

static int dir_entries_count(cached_inode *dir_inode) {
    return dir_inode->inode.file_size / sizeof(stored_dir_entry);
}

static int dir_load_entry(mounted_data *mt, cached_inode *dir_inode, int rec_no, stored_dir_entry *entry) {
    return inode_read_file_rec(mt, dir_inode, sizeof(stored_dir_entry), rec_no, entry);
}

static int dir_find_entry_by_name(mounted_data *mt, cached_inode *dir_inode, const char *name, uint32_t *inode_id, uint32_t *entry_rec_no) {
    stored_dir_entry entry;
    int recs = dir_entries_count(dir_inode);

    for (int rec_no = 0; rec_no < recs; rec_no++) {
        int err = inode_read_file_rec(mt, dir_inode, sizeof(stored_dir_entry), rec_no, &entry);
        if (err == ERR_END_OF_FILE) break;
        if (err != OK) return err;

        if (strcmp(entry.name, name) == 0) {
            *inode_id = entry.inode_id;
            if (entry_rec_no != NULL)
                *entry_rec_no = rec_no;
            return OK;
        }
    }

    return ERR_NOT_FOUND;
}

static int dir_update_entry(mounted_data *mt, cached_inode *dir_inode, int entry_rec_no, const char *name, uint32_t inode_id) {
    stored_dir_entry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_id = inode_id;

    int err = inode_write_file_rec(mt, dir_inode, sizeof(stored_dir_entry), entry_rec_no, &entry);
    if (err != OK) return err;

    return OK;
}

static int dir_append_entry(mounted_data *mt, cached_inode *dir_inode, const char *name, uint32_t inode_id) { 
    stored_dir_entry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_id = inode_id;

    uint32_t rec_no = dir_inode->inode.file_size / sizeof(stored_dir_entry);
    int err = inode_write_file_rec(mt, dir_inode, sizeof(stored_dir_entry), rec_no, &entry);
    if (err != OK) return err;

    return OK;
}

static int dir_ensure_entry_is_missing(mounted_data *mt, cached_inode *dir_inode, const char *name) {
    stored_dir_entry entry;
    int recs = dir_entries_count(dir_inode);

    for (int rec_no = 0; rec_no < recs; rec_no++) {
        int err = inode_read_file_rec(mt, dir_inode, sizeof(stored_dir_entry), rec_no, &entry);
        if (err == ERR_END_OF_FILE) break;
        if (err != OK) return err;

        if (strcmp(entry.name, name) == 0)
            return ERR_ALREADY_EXISTS;
    }

    return OK;
}

static int dir_delete_entry(mounted_data *mt, cached_inode *dir_inode, int entry_rec_no) {
    int recs_in_file = dir_inode->inode.file_size / sizeof(stored_dir_entry);
    if (entry_rec_no == recs_in_file - 1) {
        // shorten the file, but keep blocks for simplicity
        dir_inode->inode.file_size = (recs_in_file - 1) * sizeof(stored_dir_entry);
        return OK;
    }

    return dir_update_entry(mt, dir_inode, entry_rec_no, "", 0); // remove in place
}


static void dir_dump_debug_info(mounted_data *mt, cached_inode *dir_inode, int depth) {
    struct stored_dir_entry entry;
    int rec_no;
    int err;
    int recs = dir_entries_count(dir_inode);

    for (rec_no = 0; rec_no < recs; rec_no++) {
        err = dir_load_entry(mt, dir_inode, rec_no, &entry);
        if (err != OK) break;
        int is_special = (strcmp(entry.name, ".") == 0) || (strcmp(entry.name, "..") == 0);

        printf("%*s%-16s  inode:%u  ", depth * 4, "", entry.name, entry.inode_id);
        if (is_special) {
            printf("\n");
            continue;
        }

        // here, if we read from disk, the node may be already changed in the cache
        if (icache_is_inode_cached(mt, entry.inode_id)) {
            cached_inode *cached;
            err = get_cached_inode(mt, entry.inode_id, &cached);
            if (err != OK) break;
            inode_dump_debug_info("", &cached->inode);

        } else {
            stored_inode stored_node;
            err = inode_db_load(mt, entry.inode_id, &stored_node);
            if (err != OK) break;
            inode_dump_debug_info("", &stored_node);
        }

    }
    if (rec_no == 0) {
        printf("%*s(no entries)\n", depth * 4, "");
    }
}


