#include "internal.h"


static int dir_entry_find(mounted_data *mt, inode *dir_inode, const char *name, uint32_t *inode_id, uint32_t *entry_rec_no) {
    direntry entry;

    for (int rec_no = 0; rec_no < 0x7FFFFFFF; rec_no++) {
        int bytes = inode_read_file_bytes(mt, dir_inode, rec_no * sizeof(direntry), &entry, sizeof(direntry));
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

static int dir_entry_update(mounted_data *mt, inode *dir_inode, int entry_rec_no, const char *name, uint32_t inode_id) {
    direntry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_id = inode_id;

    int bytes = inode_write_file_bytes(mt, dir_inode, entry_rec_no * sizeof(direntry), &entry, sizeof(direntry));
    if (bytes < 0) return bytes;

    return OK;
}

static int dir_entry_append(mounted_data *mt, inode *dir_inode, const char *name, uint32_t inode_id) { 
    direntry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_id = inode_id;

    int bytes = inode_write_file_bytes(mt, dir_inode, dir_inode->file_size, &entry, sizeof(direntry));
    if (bytes < 0) return bytes;

    return OK;
}

static void dir_dump_debug_info(mounted_data *mt, inode *dir_inode, int depth) {
    struct direntry entry;
    inode node;
    int rec_no;
    int err, bytes;

    for (rec_no = 0; 0x7FFFFFFF; rec_no++) {
        bytes = inode_read_file_bytes(mt, dir_inode, rec_no * sizeof(direntry), &entry, sizeof(direntry));
        if (bytes < 0) break;
        int is_special = (strcmp(entry.name, ".") == 0) || (strcmp(entry.name, "..") == 0);

        printf("%*s%-16s  inode:%u  ", depth * 4, "", entry.name, entry.inode_id);
        if (is_special) {
            printf("\n");
        } else {
            err = inode_load(mt, entry.inode_id, &node);
            if (err != OK) break;

            inode_dump_debug_info("", &node);
            if (node.is_dir)
                dir_dump_debug_info(mt, &node, depth + 1);
        }
    }
    if (rec_no == 0) {
        printf("%*s(no entries)\n", depth * 4, "");
    }
}

