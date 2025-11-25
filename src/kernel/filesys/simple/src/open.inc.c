#include "internal.h"
#include <string.h>


static int open_inodes_allocate(mounted_data *mt, inode *iptr, uint32_t inode_rec_no, open_inode **open_inode_ptr) {
    // find if already open
    int index = -1;
    for (int i = 0; i < MAX_OPEN_INODES; i++) {
        if (mt->open_inodes[i].is_used && mt->open_inodes[i].inode_db_rec_no == inode_rec_no) {
            index = i;
            break;
        }
    }

    // not found, try to find an empty slot
    if (index == -1) {
        for (int i = 0; i < MAX_OPEN_INODES; i++) {
            if (mt->open_inodes[i].is_used) {
                index = i;
                break;
            }
        }
        if (index == -1)
            return ERR_RESOURCES_EXHAUSTED; // too many open files
        
        mt->open_inodes[index].is_used = 1;
        mt->open_inodes[index].ref_count = 0;
    }

    // initialize runtime info
    memcpy(&mt->open_inodes[index].inode_in_mem, iptr, sizeof(inode));
    mt->open_inodes[index].inode_db_rec_no = inode_rec_no;
    mt->open_inodes[index].is_dirty = 0;

    *open_inode_ptr = &mt->open_inodes[index];
    return OK;
}

static int open_inodes_release(mounted_data *mt, open_inode *oinode_ptr) {
    if (oinode_ptr->is_dirty) {
        if (oinode_ptr->inode_db_rec_no == ROOT_DIR_INODE_REC_NO) {
            // save in superblock, in memory
            memcpy(&mt->superblock->root_dir_inode, &oinode_ptr->inode_in_mem, sizeof(inode));
        } else {
            // save in inodes database
            int err = inode_write_file_record(mt, &mt->superblock->inodes_db_inode, sizeof(inode), oinode_ptr->inode_db_rec_no, &oinode_ptr->inode_in_mem);
            if (err != OK) return err;
        }
    }

    oinode_ptr->is_used = 0;
    return OK;
}

static int open_files_allocate(mounted_data *mt, open_inode *oinode, open_file **ofile_ptr) {
    // find a free slot
    int index = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!mt->open_files[i].is_used) {
            index = i;
            break;
        }
    }
    if (index == -1)
        return ERR_RESOURCES_EXHAUSTED;

    mt->open_files[index].is_used = 1;
    mt->open_files[index].inode = oinode;
    mt->open_files[index].file_position = 0;
    mt->open_files[index].inode->ref_count += 1; // one more references
    
    *ofile_ptr = &mt->open_files[index];
    return OK;
}

static int open_files_release(mounted_data *mt, open_file *ofile) {
    ofile->inode->ref_count -= 1;

    if (ofile->inode->ref_count == 0) {
        int err = open_inodes_release(mt, ofile->inode);
        if (err != OK) return err;
    }

    ofile->is_used = 0;
    return OK;
}
