#ifndef _FAT_DIR_OPS_C
#define _FAT_DIR_OPS_C

#include <drivers/clock.h>
#include <filesys/vfs.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <klib/path.h>
#include <klog.h>
#include <errors.h>
#include "fat_priv.h"




// abstract away FAT 16 / 32 differences in root dir
static int priv_dir_open_root(fat_info *fat, fat_priv_dir_info **ppd) {
    klog_trace("priv_dir_open_root()");
    if (fat->fat_type == FAT32)
        return priv_dir_open_cluster(fat, fat->boot_sector->types.fat_32.root_dir_cluster, ppd);

    fat_priv_dir_info *pd = kmalloc(sizeof(fat_priv_dir_info));
    *ppd = pd;
    memset(pd, 0, sizeof(fat_priv_dir_info));
    pd->is_fat16_root = true;
    pd->fat16_root_data.sector_buffer = kmalloc(fat->bytes_per_sector);

    // need to read first sector
    // a sample value is 32 sectors for the root directory.
    // therefore the root dir total size is 16KB.

    int err = fat->partition->dev->ops->read(fat->partition->dev,
        fat->root_dir_starting_lba, 0, 1, pd->fat16_root_data.sector_buffer);
    if (err) return err;

    pd->fat16_root_data.sector_no = 0;
    pd->fat16_root_data.offset_in_sector = 0;
    pd->fat16_root_data.sector_dirty = false;
    pd->fat16_root_data.loaded = false;

    klog_debug("priv_dir_open_root(), read %d sectors at LBA %d", 1, fat->root_dir_starting_lba);
    klog_debug_hex(pd->fat16_root_data.sector_buffer, fat->bytes_per_sector, 0);

    return SUCCESS;
}

static int priv_dir_open_cluster(fat_info *fat, uint32_t cluster_no, fat_priv_dir_info **ppd) {
    klog_trace("priv_dir_open_cluster(cluster=%d)", cluster_no);
    fat_priv_dir_info *pd = kmalloc(sizeof(fat_priv_dir_info));
    *ppd = pd;
    memset(pd, 0, sizeof(fat_priv_dir_info));
    pd->is_fat16_root = false; // explicitly

    // we treat dir contents as file contents
    uint32_t dir_size = fat->bytes_per_cluster; // can be much larger...
    return fat->ops->priv_file_open(fat, cluster_no, dir_size, &(pd->pf));
}

static int priv_dir_find_and_open(fat_info *fat,  uint8_t *path, bool containing_dir, fat_priv_dir_info **ppd) {
    int err;
    fat_dir_entry *entry = NULL;

    bool is_root = strlen(path) == 1 && path[0] == '/';
    if (is_root && containing_dir) {
        klog_error("priv_dir_find_and_open(): Cannot find containing of root dir! (path=\"%s\")", path);
        err = ERR_BAD_ARGUMENT;
        goto exit;
    }

    int path_parts = count_path_parts(path);
    if (is_root || (path_parts == 1 && containing_dir)) {
        err = fat->ops->priv_dir_open_root(fat, ppd);
        if (err) goto exit;
    } else {
        entry = kmalloc(sizeof(fat_dir_entry));
        err = fat->ops->find_path_dir_entry(fat, path, containing_dir, entry);
        if (err) goto exit;
        
        if (!entry->attributes.flags.directory) {
            err = ERR_NOT_A_DIRECTORY;
            goto exit;
        }

        uint32_t cluster_no = entry->first_cluster_no;
        err = fat->ops->priv_dir_open_cluster(fat, cluster_no, ppd);
        if (err) goto exit;
    }

    err = SUCCESS;
exit:
    if (entry != NULL)
        kfree(entry);
    return err;
}

static int priv_dir_read_slot(fat_info *fat, fat_priv_dir_info *pd, uint8_t *buffer32) {
    klog_trace("priv_dir_read_slot()");
    int err;
    if (!pd->is_fat16_root) {
        err = fat->ops->priv_file_read(fat, pd->pf, buffer32, BYTES_PER_DIR_SLOT);
        return (err < 0) ? err : SUCCESS; // we don't care how many bytes we read
    }

    // fat 16 root code here
    if (pd->fat16_root_data.offset_in_sector >= fat->bytes_per_sector) {
        // need to read the next sector
        if (pd->fat16_root_data.sector_no == fat->root_dir_sectors_count - 1)
            return ERR_NO_MORE_CONTENT;
        
        // before reading, write any dirty sector
        if (pd->fat16_root_data.sector_dirty) {
            err = fat->partition->dev->ops->write(fat->partition->dev,
                fat->root_dir_starting_lba + pd->fat16_root_data.sector_no, 
                0, 1, pd->fat16_root_data.sector_buffer);
            if (err < 0) return err;
        }

        // now we can read the next sector
        err = fat->partition->dev->ops->read(fat->partition->dev,
            fat->root_dir_starting_lba + pd->fat16_root_data.sector_no + 1, 
            0, 1, pd->fat16_root_data.sector_buffer);
        if (err < 0) return err;
        pd->fat16_root_data.sector_no += 1;
        pd->fat16_root_data.offset_in_sector = 0;
        pd->fat16_root_data.loaded = true;
    }

    // now we can read
    memcpy(buffer32, pd->fat16_root_data.sector_buffer + pd->fat16_root_data.offset_in_sector, BYTES_PER_DIR_SLOT);
    pd->fat16_root_data.offset_in_sector += BYTES_PER_DIR_SLOT;

    return SUCCESS;
}

static int priv_dir_write_slot(fat_info *fat, fat_priv_dir_info *pd, uint8_t *buffer32) {
    klog_trace("priv_dir_write_slot()");
    int err;
    if (!pd->is_fat16_root) {
        err = fat->ops->priv_file_write(fat, pd->pf, buffer32, BYTES_PER_DIR_SLOT);
        return (err < 0) ? err : SUCCESS; // we don't care how many bytes we read
    }

    // fat 16 root code here
    if (pd->fat16_root_data.offset_in_sector >= fat->bytes_per_sector) {
        // need to read the next sector
        if (pd->fat16_root_data.sector_no == fat->root_dir_sectors_count - 1)
            return ERR_NO_SPACE_LEFT;
        
        // before reading, write any dirty sector
        if (pd->fat16_root_data.sector_dirty) {
            err = fat->partition->dev->ops->write(fat->partition->dev,
                fat->root_dir_starting_lba + pd->fat16_root_data.sector_no, 
                0, 1, pd->fat16_root_data.sector_buffer);
            if (err < 0) return err;
        }

        // read next sector
        int err = fat->partition->dev->ops->read(fat->partition->dev,
            fat->root_dir_starting_lba + pd->fat16_root_data.sector_no + 1, 
            0, 1, pd->fat16_root_data.sector_buffer);
        if (err < 0) return err;
        pd->fat16_root_data.sector_no += 1;
        pd->fat16_root_data.offset_in_sector = 0;
        pd->fat16_root_data.loaded = true;
    }

    // now we can write
    memcpy(pd->fat16_root_data.sector_buffer + pd->fat16_root_data.offset_in_sector, buffer32, BYTES_PER_DIR_SLOT);
    pd->fat16_root_data.offset_in_sector += BYTES_PER_DIR_SLOT;
    pd->fat16_root_data.sector_dirty = true;

    return SUCCESS;
}

static int priv_dir_get_slot_no(fat_info *fat, fat_priv_dir_info *pd) {
    int slot_no;
    if (!pd->is_fat16_root) {
        slot_no = fat->ops->priv_file_seek(fat, pd->pf, 0, SEEK_CURRENT) / BYTES_PER_DIR_SLOT;

    } else {
        slot_no = (pd->fat16_root_data.sector_no * fat->bytes_per_sector 
                    + pd->fat16_root_data.offset_in_sector) / BYTES_PER_DIR_SLOT;
    }

    klog_trace("priv_dir_get_slot_no() -> %d", slot_no);
    return slot_no;
}

static int priv_dir_seek_slot(fat_info *fat, fat_priv_dir_info *pd, int slot_no) {
    klog_trace("priv_dir_seek_slot(slot_no=%d)", slot_no);
    if (!pd->is_fat16_root)
        return fat->ops->priv_file_seek(fat, pd->pf, slot_no * BYTES_PER_DIR_SLOT, SEEK_START);

    // fat 16 root code here
    uint32_t target_sector_no = (slot_no * BYTES_PER_DIR_SLOT) / fat->bytes_per_sector;
    bool already_loaded = pd->fat16_root_data.loaded && pd->fat16_root_data.sector_no == target_sector_no;
    if (!already_loaded) {
        // we need to read the new sector.
        if (target_sector_no >= fat->root_dir_sectors_count)
            return ERR_NO_MORE_CONTENT;

        // before reading, write any dirty sector
        if (pd->fat16_root_data.sector_dirty) {
            int err = fat->partition->dev->ops->write(fat->partition->dev,
                fat->root_dir_starting_lba + pd->fat16_root_data.sector_no, 
                0, 1, pd->fat16_root_data.sector_buffer);
            if (err) return err;
        }

        // read target sector
        int err = fat->partition->dev->ops->read(fat->partition->dev,
            fat->root_dir_starting_lba + pd->fat16_root_data.sector_no + target_sector_no, 
            0, 1, pd->fat16_root_data.sector_buffer);
        if (err) return err;
        pd->fat16_root_data.sector_no = target_sector_no;
        pd->fat16_root_data.loaded = true;
    }

    pd->fat16_root_data.offset_in_sector = (slot_no * BYTES_PER_DIR_SLOT) % fat->bytes_per_sector;

    return SUCCESS;
}

static int priv_dir_close(fat_info *fat, fat_priv_dir_info *pd) {
    klog_trace("priv_dir_close(fat=%p, pd=%p)", fat, pd);
    int err;
    if (pd->is_fat16_root) {
        // before closing, write any dirty sector
        if (pd->fat16_root_data.sector_dirty) {
            int err = fat->partition->dev->ops->write(fat->partition->dev,
                fat->root_dir_starting_lba + pd->fat16_root_data.sector_no, 
                0, 1, pd->fat16_root_data.sector_buffer);
            if (err) return err;
        }

        // free whatever we allocated
        kfree(pd->fat16_root_data.sector_buffer);
    } else {
        err = fat->ops->priv_file_close(fat, pd->pf);
        if (err) return err;
    }
    kfree(pd);
    return SUCCESS;
}

static int priv_dir_read_one_entry(fat_info *fat, fat_priv_dir_info *pd, fat_dir_entry *entry) {
    klog_trace("priv_dir_read_one_entry(fat=%p, pd=%p, entry=%p)", fat, pd, entry);
    uint8_t buffer32[BYTES_PER_DIR_SLOT];

    while (true) {
        int slot_before_reading = priv_dir_get_slot_no(fat, pd);
        int err = priv_dir_read_slot(fat, pd, buffer32);
        if (err < 0) return err;

        klog_debug("read slot contents follow");
        klog_debug_hex(buffer32, BYTES_PER_DIR_SLOT, 0);

        if (is_dir_slot_eof(buffer32))
            return ERR_NO_MORE_CONTENT;

        if (is_dir_slot_deleted(buffer32))
            continue;

        if (is_dir_slot_long_name(buffer32))
            continue; // not supported for now

        // this helps for fast updates (size, date etc)
        entry->short_entry_slot_no = slot_before_reading;
        dir_slot_to_entry(buffer32, entry);
        break;
    }

    return SUCCESS;
}

static int priv_dir_create_entry(fat_info *fat, fat_priv_dir_info *pd, char *name, bool directory) {
    klog_trace("priv_dir_create_entry(name=\"%s\", dir=%s)", name, directory ? "true" : "false");
    uint8_t buffer32[BYTES_PER_DIR_SLOT];
    uint32_t slot_no;
    int err;
    fat_dir_entry *entry = NULL;

    // prepare the new entry
    entry = kmalloc(sizeof(fat_dir_entry));
    memset(entry, 0, sizeof(fat_dir_entry));
    strncpy(entry->short_name, name, 12 + 1);
    entry->attributes.flags.directory = directory;
    real_time_clock_info_t time;
    get_real_time_clock(&time);
    dir_entry_set_created_time(entry, &time);
    dir_entry_set_modified_time(entry, &time);
    
    // creating an empty file does not allocate a new cluster.
    // not so sure about directories though...
    entry->first_cluster_no = 0;
    entry->file_size = 0;

    // read through all the entries to find an empty slot
    // later, to save long names, we'll need to find lots of consecutive free slots.
    fat->ops->priv_dir_seek_slot(fat, pd, 0);
    while (true) {
        slot_no = priv_dir_get_slot_no(fat, pd);
        int err = priv_dir_read_slot(fat, pd, buffer32);
        if (err != NO_ERROR && err != ERR_NO_MORE_CONTENT) goto exit;

        klog_debug("priv_dir_read_slot() --> slot contents follow");
        klog_debug_hex(buffer32, BYTES_PER_DIR_SLOT, 0);

        // we need to find eof, though we do not know how we'll allocate more space
        bool passed_entries = is_dir_slot_eof(buffer32) || err == ERR_NO_MORE_CONTENT;
        if (!passed_entries)
            continue;

        // now we can save our new entry!
        err = priv_dir_seek_slot(fat, pd, slot_no);
        if (err) goto exit;
        entry->short_entry_slot_no = slot_no;

        // we can save long name here too
        dir_entry_to_slot(entry, buffer32);
        err = priv_dir_write_slot(fat, pd, buffer32);
        if (err) goto exit;
        break;
    }

    err = SUCCESS;
exit:
    if (entry != NULL)
        kfree(entry);
    return err;
}

static int priv_dir_entry_invalidate(fat_info *fat, fat_priv_dir_info *pd, fat_dir_entry *entry) {
    klog_trace("priv_dir_entry_invalidate(name=\"%s\")", entry->short_name);
    uint8_t buffer32[BYTES_PER_DIR_SLOT];
    int err;

    klog_debug("current slot is %d", priv_dir_get_slot_no(fat, pd));
    klog_debug("going to slot %d", entry->short_entry_slot_no);

    err = fat->ops->priv_dir_seek_slot(fat, pd, entry->short_entry_slot_no);
    if (err) return err;

    klog_debug("after seek, slot is %d", priv_dir_get_slot_no(fat, pd));
    
    err = priv_dir_read_slot(fat, pd, buffer32);
    if (err) return err;

    klog_debug("Read buffer for slot, contents follow");
    klog_debug_hex(buffer32, 32, 0);

    // fingers crossed this is the correct entry...
    dir_slot_mark_deleted(buffer32);

    klog_debug("Marking entry named \"%s\" as deleted, new buffer follows", entry->short_name);
    klog_debug_hex(buffer32, 32, 0);

    err = fat->ops->priv_dir_seek_slot(fat, pd, entry->short_entry_slot_no);
    if (err) return err;

    err = priv_dir_write_slot(fat, pd, buffer32);
    if (err) return err;

    return SUCCESS;
}


// find entry in a directory
static int find_entry_in_dir(fat_info *fat, fat_priv_dir_info *pd, uint8_t *name, fat_dir_entry *entry) {
    klog_trace("find_entry_in_dir(name=\"%s\")", name);

    int err = priv_dir_seek_slot(fat, pd, 0);
    if (err) return err;

    while (true) {
        err = priv_dir_read_one_entry(fat, pd, entry);
        if (err == ERR_NO_MORE_CONTENT)
            return ERR_NOT_FOUND;
        else if (err)
            return err;

        klog_debug("find_entry_in_dir(): gotten entry for \"%s\"", entry->short_name);
        if (strcmp(entry->short_name, name) == 0)
            break;
    }
    
    return SUCCESS;
}

// find the dir_entry for the containing dir of the path. we can then open it to perform operations.
static int find_path_dir_entry(fat_info *fat,  uint8_t *path, bool containing_dir, fat_dir_entry *entry) {
    klog_trace("find_path_dir_entry(path=\"%s\", containing=%s)", path, containing_dir ? "yes" : "no");
    fat_priv_dir_info *pd = NULL;
    int offset = 0;
    int err = NO_ERROR;
    char *name = kmalloc(strlen(path) + 1);

    int parts_count = count_path_parts(path);
    int parts_needed = containing_dir ? parts_count - 1 : parts_count;
    if (parts_needed == 0) {
        klog_error("find_path_dir_entry(): containing dir for path \"%s\" could be root", path);
        err = ERR_BAD_ARGUMENT;
        goto exit;
    }
    int parts_so_far = 0;

    // start from root (or maybe current working directory?)
    err = priv_dir_open_root(fat, &pd);
    if (err) goto exit;

    while (true) {
        // extract a name from the path
        err = get_next_path_part(path, &offset, name);
        if (err) goto exit;
        parts_so_far++;
        
        // need chunk of user name
        err = find_entry_in_dir(fat, pd, name, entry);
        if (err == ERR_NO_MORE_CONTENT) {
            err = ERR_NOT_FOUND;
            goto exit;
        } else if (err) {
            goto exit;
        }

        // since we found entry of this level, close it
        err = priv_dir_close(fat, pd);
        if (err) goto exit;
        pd = NULL;

        if (parts_so_far == parts_needed) {
            // we are done!
            err = SUCCESS;
            break;
        }
        
        // what we are about to open should be a directory
        if (!entry->attributes.flags.directory) {
            err = ERR_NOT_A_DIRECTORY;
            goto exit;
        }

        // close this and open the next level of directory
        err = priv_dir_open_cluster(fat, entry->first_cluster_no, &pd);
        if (err) goto exit;
    }

    // close any dangling open dir
    if (pd != NULL) {
        err = priv_dir_close(fat, pd);
        if (err) goto exit;
    }

exit:
    if (name)
        kfree(name);
    return err;
}


#endif