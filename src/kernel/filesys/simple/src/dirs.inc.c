#include "internal.h"


static int dir_entry_find(mounted_data *mt, inode *dir_inode, const char *name, uint32_t *inode_rec_no, int *enrty_rec_no) {
    dir_entry entry;

    for (int rec_no = 0; rec_no < 0x7FFFFFFF; rec_no++) {
        int bytes = inode_read_file_data(mt, dir_inode, rec_no * sizeof(dir_entry), &entry, sizeof(dir_entry));
        if (bytes == ERR_END_OF_FILE) break;
        if (bytes < 0) return bytes;

        if (strcmp(entry.name, name) == 0) {
            *inode_rec_no = entry.inode_rec_no;
            if (enrty_rec_no != NULL)
                *enrty_rec_no = rec_no;
            return OK;
        }
    }

    return ERR_NOT_FOUND;
}

static int dir_entry_update(mounted_data *mt, inode *dir_inode, int enrty_rec_no, const char *name, uint32_t inode_rec_no) {
    dir_entry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_rec_no = inode_rec_no;

    int bytes = inode_write_file_data(mt, dir_inode, enrty_rec_no * sizeof(dir_entry), &entry, sizeof(dir_entry));
    if (bytes < 0) return bytes;

    return OK;
}

static int dir_entry_append(mounted_data *mt, inode *dir_inode, const char *name, uint32_t inode_rec_no) { 
    dir_entry entry;

    strncpy(entry.name, name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = 0;
    entry.inode_rec_no = inode_rec_no;

    int bytes = inode_write_file_data(mt, dir_inode, dir_inode->file_size, &entry, sizeof(dir_entry));
    if (bytes < 0) return bytes;

    return OK;
}

static void dir_dump_debug_info(mounted_data *mt, inode *dir_inode, int depth) {
    struct dir_entry entry;
    inode node;
    int rec_no;
    int err, bytes;

    for (rec_no = 0; 0x7FFFFFFF; rec_no++) {
        bytes = inode_read_file_data(mt, dir_inode, rec_no * sizeof(dir_entry), &entry, sizeof(dir_entry));
        if (bytes < 0) break;

        err = inode_load(mt, entry.inode_rec_no, &node);
        if (err != OK) break;

        printf("%*s%-24s  inode:%d  ", depth * 4, "", entry.name, entry.inode_rec_no);
        inode_dump_debug_info("", &node);

        if (node.is_dir) {
            dir_dump_debug_info(mt, &node, depth + 1);
        }
    }
    if (rec_no == 0) {
        printf("%*s(no entries)\n", depth * 4, "");
    }
}

