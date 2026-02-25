#include "sfs_internal.h"
#include "../../../klib/string.h"



error_t sfs_node_dir_find_entry(sfs_mount_data *mt, stored_inode *sin, const char *name, uint32_t *rec_no, uint32_t *inode_no) {
    uint32_t n = 0;
    stored_dir_entry entry;
    while (true) {
        ssize_t bytes = sfs_node_read_file_rec(mt, sin, sizeof(stored_dir_entry), n, &entry);
        if (bytes == 0 || bytes == ERR_EOF) break;

        if (strcmp(entry.name, name) == 0) {
            if (rec_no != NULL) *rec_no = n;
            if (inode_no != NULL) *inode_no = entry.inode_num;
            return OK;
        }
        n++;
    }
    return ERR_NOT_FOUND;
}

error_t sfs_node_dir_set_entry(sfs_mount_data *mt, inode_no_t dir_node_no, stored_inode *dir_node, uint32_t rec_no, const char *name, uint32_t inode_no) {
    stored_dir_entry entry;
    strncpy(entry.name, name, sizeof(entry.name));
    entry.inode_num = inode_no;

    ssize_t bytes = sfs_node_write_file_rec(mt, dir_node, dir_node_no, sizeof(stored_dir_entry), rec_no, &entry);
    if (bytes < 0) return (error_t)bytes;
    if ((size_t)bytes < sizeof(stored_dir_entry)) return ERR_CORRUPTION_DETECTED;

    return OK;
}

error_t sfs_node_dir_add_entry(sfs_mount_data *mt, inode_no_t dir_node_no, stored_inode *dir_node, const char *name, uint32_t inode_no) {
    uint32_t records = (uint32_t)(dir_node->file_size / sizeof(stored_dir_entry));
    return sfs_node_dir_set_entry(mt, dir_node_no, dir_node, records, name, inode_no);
}

error_t sfs_node_dir_is_empty(sfs_mount_data *mt, stored_inode *sin, bool ignore_special_dirs, bool *is_empty) {
    uint32_t count = 0;
    uint32_t rec = 0;
    stored_dir_entry entry;
    while (true) {
        ssize_t bytes = sfs_node_read_file_rec(mt, sin, sizeof(stored_dir_entry), rec, &entry);
        if (bytes == 0 || bytes == ERR_EOF) break;
        if (stored_dir_entry_is_unused(&entry))
            continue;

        if (ignore_special_dirs && (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0))
            continue;

        count++;
        rec++;
    }

    *is_empty = (count == 0);
    return OK;
}

error_t sfs_inodes_db_append(sfs_mount_data *mt, stored_inode *dir_node, uint32_t *inode_no) {
    stored_inode *inodes_db;
    error_t err = mt->inode_cache->ops->get(mt->inode_cache, INODE_DB_INODE_ID, (void **)&inodes_db);
    if (err) return err;

    uint32_t records = (uint32_t)(inodes_db->file_size / sizeof(stored_inode));
    ssize_t bytes = sfs_node_write_file_rec(mt, inodes_db, INODE_DB_INODE_ID, sizeof(stored_inode), records, dir_node);
    if (bytes < 0) return (error_t) bytes;
    if (bytes != sizeof(stored_inode)) return ERR_CORRUPTION_DETECTED;

    *inode_no = records;
    return OK;
}
