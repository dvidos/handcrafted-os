#include <errors.h>
#include <klib/string.h>
#include "fat_priv.h"
#include "fat_dir_ops.c"
#include "fat_file_ops.c"


// needed to lock for writes, this should be per logical volume....
lock_t fat_write_lock;

static int fat_supported(struct partition *partition) {
    klog_trace("FAT probing partition #%d (legacy type 0x%02x)", partition->part_no, partition->legacy_type);
    int err;

    if (partition->dev->ops->sector_size(partition->dev) != 512) {
        klog_error("Only devices with 512 bytes sectors are supported");
        return ERR_NOT_SUPPORTED;
    }
    if (sizeof(fat_boot_sector_t) != 512) {
        klog_error("Boot sector struct would not align with reading of a sector");
        return ERR_NOT_SUPPORTED;
    }
    
    // check the first sector of the partition.
    fat_boot_sector_t *boot_sector = kmalloc(sizeof(fat_boot_sector_t));
    memset(boot_sector, 0, sizeof(fat_boot_sector_t));
    err = partition->dev->ops->read(partition->dev, partition->first_sector, 0, 1, (char *)boot_sector);
    if (err) {
        klog_debug("Err %d reading first sector of partition", err);
        kfree(boot_sector);
        return err;
    }
    
    // berify some magic numbers
    if (boot_sector->bootjmp[0] != 0xEB ||
        boot_sector->bytes_per_sector != 512 ||
        boot_sector->number_of_fats != 2 ||
        (boot_sector->total_sectors_16bits == 0 && boot_sector->total_sectors_32bits == 0) ||
        (
            boot_sector->types.fat_12_16.boot_signature != 0x28 &&
            boot_sector->types.fat_12_16.boot_signature != 0x29 && 
            boot_sector->types.fat_32.boot_signature != 0x28 &&
            boot_sector->types.fat_32.boot_signature != 0x29
        ) ||
        boot_sector->bootable_sector_signature != 0xAA55
    ) {
        klog_debug("Does not look like a FAT MBR");
        kfree(boot_sector);
        return ERR_NOT_SUPPORTED;
    }

    if (boot_sector->types.fat_12_16.boot_signature == 0x28 || 
        boot_sector->types.fat_12_16.boot_signature == 0x29) {
        klog_debug("FAT12/16 detected");
        err = SUCCESS;
    } else if (boot_sector->types.fat_32.boot_signature == 0x28 ||
        boot_sector->types.fat_32.boot_signature == 0x29) {
        klog_debug("FAT32 detected");
        err = SUCCESS;
    } else {
        klog_debug("Neither FAT12/16, nor FAT32, unsupported");
        err = ERR_NOT_SUPPORTED;
    }

    kfree(boot_sector);
    return err;
}


static int fat_open_superblock(struct partition *partition, struct superblock *superblock) {
    klog_trace("fat_open_superblock(dev %d, part %d)", partition->dev->dev_no, partition->part_no);

    int err = fat_supported(partition);
    if (err)
        return err;

    fat_info *fat = NULL;
    fat_boot_sector_t *boot_sector = kmalloc(sizeof(fat_boot_sector_t));
    memset(boot_sector, 0, sizeof(fat_boot_sector_t));
    err = partition->dev->ops->read(partition->dev, partition->first_sector, 0, 1, (char *)boot_sector);
    if (err) {
        klog_debug("Err %d reading first sector of partition", err);
        goto error;
    }
    
    if (boot_sector->bootjmp[0] != 0xEB ||
        boot_sector->bytes_per_sector != 512 ||
        boot_sector->number_of_fats != 2 ||
        (boot_sector->total_sectors_16bits == 0 && boot_sector->total_sectors_32bits == 0) ||
        (
            boot_sector->types.fat_12_16.boot_signature != 0x28 &&
            boot_sector->types.fat_12_16.boot_signature != 0x29 && 
            boot_sector->types.fat_32.boot_signature != 0x28 &&
            boot_sector->types.fat_32.boot_signature != 0x29
        ) ||
        boot_sector->bootable_sector_signature != 0xAA55
    ) {
        klog_debug("Does not look like a FAT MBR");
        err = ERR_NOT_SUPPORTED;
        goto error;
    }

    // fat_info will allow all future operations
    fat = kmalloc(sizeof(fat_info));
    memset(fat, 0, sizeof(fat_info));
    fat->partition = partition;
    fat->boot_sector = boot_sector;

    if (boot_sector->types.fat_12_16.boot_signature == 0x28 || 
        boot_sector->types.fat_12_16.boot_signature == 0x29
    ) {
        // now we know we are 12/16, find which one is it
        int root_dir_sectors = ((boot_sector->root_entry_count * 32) + boot_sector->bytes_per_sector - 1) / boot_sector->bytes_per_sector;
        int data_sectors = boot_sector->total_sectors_16bits - (boot_sector->reserved_sector_count + (boot_sector->sectors_per_fat_16 * boot_sector->number_of_fats) + root_dir_sectors);
        int data_clusters = data_sectors / boot_sector->sectors_per_cluster;

        // a fat16 cannot have less than 4085 clusters
        fat->fat_type = data_clusters < 4085 ? FAT12 : FAT16;
        klog_debug("FAT%d detected", fat->fat_type == FAT12 ? 12 : 16);

    } else if (boot_sector->types.fat_32.boot_signature == 0x28 ||
        boot_sector->types.fat_32.boot_signature == 0x29
    ) {
        klog_debug("FAT32 detected");
        fat->fat_type = FAT32;
    } else {
        klog_debug("Neither FAT12/16, nor FAT32, unsupported");
        err = ERR_NOT_SUPPORTED;
        goto error;
    }

    fat->bytes_per_sector = boot_sector->bytes_per_sector;
    fat->sectors_per_cluster = boot_sector->sectors_per_cluster;
    fat->bytes_per_cluster = fat->bytes_per_sector * fat->sectors_per_cluster;
    // round largest sector up to cluster boundary
    fat->largest_cluster_no = (partition->num_sectors + boot_sector->sectors_per_cluster - 1) / boot_sector->sectors_per_cluster;

    fat->sectors_per_fat = fat->fat_type == FAT32 ? 
        boot_sector->types.fat_32.sectors_per_fat_32 : 
        boot_sector->sectors_per_fat_16;

    fat->fat_starting_lba = 
        partition->first_sector + 
        boot_sector->reserved_sector_count;

    // on FAT12/16, the rood dir is fixed size, immediately after the FATs, data clusters start after that
    // on FAT32 we are given a specific cluster number, it resides in data clusters area
    uint32_t end_of_fats_lba = 
            partition->first_sector + 
            boot_sector->reserved_sector_count +
            (fat->sectors_per_fat * boot_sector->number_of_fats);
    uint32_t fat1216_root_dir_sectors_count = 
            (boot_sector->root_entry_count * 32) / boot_sector->bytes_per_sector;

    if (fat->fat_type == FAT12 || fat->fat_type == FAT16) {
        fat->data_clusters_starting_lba = end_of_fats_lba + fat1216_root_dir_sectors_count;
        fat->root_dir_starting_lba = end_of_fats_lba;
        fat->root_dir_sectors_count = fat1216_root_dir_sectors_count;
        fat->end_of_chain_value = fat->fat_type == FAT12 ? 0xFF8 : 0xFFF8;
    } else {
        fat->data_clusters_starting_lba = end_of_fats_lba;
        fat->root_dir_starting_lba =
            fat->data_clusters_starting_lba +
            (boot_sector->types.fat_32.root_dir_cluster - 2) * fat->sectors_per_cluster;
        fat->root_dir_sectors_count = fat->sectors_per_cluster; // no upper size limit actually
        fat->end_of_chain_value = 0x0FFFFFF8;
    }

    // setup operations for ease of use
    fat->ops = kmalloc(sizeof(struct fat_operations));
    memset(fat->ops, 0, sizeof(struct fat_operations));

    fat->ops->read_fat_sector = read_fat_sector;
    fat->ops->write_fat_sector = write_fat_sector;
    fat->ops->get_fat_entry_value = get_fat_entry_value;
    fat->ops->set_fat_entry_value = set_fat_entry_value;
    fat->ops->is_end_of_chain_entry_value = is_end_of_chain_entry_value;
    fat->ops->find_a_free_cluster = find_a_free_cluster;
    fat->ops->get_n_index_cluster_no = get_n_index_cluster_no;
    
    fat->ops->read_data_cluster = read_data_cluster;
    fat->ops->write_data_cluster = write_data_cluster;

    fat->ops->ensure_first_cluster_allocated = ensure_first_cluster_allocated;
    fat->ops->move_to_next_data_cluster = move_to_next_data_cluster;
    fat->ops->move_to_n_index_data_cluster = move_to_n_index_data_cluster;

    fat->ops->priv_file_open = priv_file_open;
    fat->ops->priv_file_read = priv_file_read;
    fat->ops->priv_file_write = priv_file_write;
    fat->ops->priv_file_seek = priv_file_seek;
    fat->ops->priv_file_close = priv_file_close;

    fat->ops->priv_dir_open_root = priv_dir_open_root;
    fat->ops->priv_dir_open_cluster = priv_dir_open_cluster;
    fat->ops->priv_dir_find_and_open = priv_dir_find_and_open;
    fat->ops->priv_dir_read_slot = priv_dir_read_slot;
    fat->ops->priv_dir_write_slot = priv_dir_write_slot;
    fat->ops->priv_dir_get_slot_no = priv_dir_get_slot_no;
    fat->ops->priv_dir_seek_slot = priv_dir_seek_slot;
    fat->ops->priv_dir_close = priv_dir_close;

    fat->ops->priv_dir_read_one_entry = priv_dir_read_one_entry;
    fat->ops->priv_dir_create_entry = priv_dir_create_entry;
    fat->ops->priv_dir_entry_invalidate = priv_dir_entry_invalidate;

    fat->ops->find_path_dir_entry = find_path_dir_entry;
    fat->ops->find_entry_in_dir = find_entry_in_dir;
    
    debug_fat_info(fat);

    superblock->partition = partition;
    superblock->ops = fat_get_file_operations();
    superblock->priv_fs_driver_data = fat;

    return SUCCESS;

error:
    if (boot_sector != NULL) kfree(boot_sector);
    if (fat->ops != NULL) kfree(fat->ops);
    if (fat != NULL) kfree(fat);
    return err;
}

static int fat_close_superblock(struct superblock *superblock) {
    klog_trace("fat_close_superblock()");

    // ideally we'll sync if needed

    fat_info *fat = (fat_info *)superblock->priv_fs_driver_data;
    kfree(fat->ops);
    kfree(fat);

    return SUCCESS;
}

static int fat_open(char *path, file_t *file) {
    klog_trace("fat_open(path=\"%s\")", path);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    int err;

    fat_dir_entry entry;
    err = fat->ops->find_path_dir_entry(fat, path, false, &entry);
    if (err) return err;
    if (entry.attributes.flags.directory || entry.attributes.flags.volume_label)
        return ERR_NOT_A_FILE;
    
    fat_priv_file_info *pf = NULL;
    err = fat->ops->priv_file_open(fat, entry.first_cluster_no, entry.file_size, &pf);
    if (err)
        return err;
    file->fs_driver_priv_data = pf;

    return SUCCESS;
}

static int fat_read(file_t *file, char *buffer, int length) {
    klog_trace("fat_read(length=%d)", length);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    fat_priv_file_info *pf = (fat_priv_file_info *)file->fs_driver_priv_data;
    return fat->ops->priv_file_read(fat, pf, buffer, length);
}

static int fat_write(file_t *file, char *buffer, int length) {
    klog_trace("fat_write(length=%d)", length);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    fat_priv_file_info *pf = (fat_priv_file_info *)file->fs_driver_priv_data;
    acquire(&fat_write_lock);
    int err = fat->ops->priv_file_write(fat, pf, buffer, length);
    release(&fat_write_lock);
    return err;
}

static int fat_seek(file_t *file, int offset, enum seek_origin origin) {
    klog_trace("fat_seek(offset=%d, origin=%d)", offset, origin);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    fat_priv_file_info *pf = (fat_priv_file_info *)file->fs_driver_priv_data;
    return fat->ops->priv_file_seek(fat, pf, offset, origin);
}

static int fat_close(file_t *file) {
    klog_trace("fat_close()");
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    fat_priv_file_info *pf = (fat_priv_file_info *)file->fs_driver_priv_data;
    return fat->ops->priv_file_close(fat, pf);
}


static int fat_open_root_dir(struct superblock *sb, file_t *file) {
    klog_trace("fat_open_root_dir()");
    fat_info *fat = (fat_info *)sb->priv_fs_driver_data;

    file->superblock = sb;
    file->driver = sb->driver;
    file->partition = sb->partition;
    file->storage_dev = sb->partition->dev;
    file->path = "/";
    
    // need to signify to other that this is the root directory

    // 
    if (fat->fat_type == FAT32) {
        return ERR_NOT_IMPLEMENTED;
        // solve this when we implement the normal open_dir for FAT
        // return priv_dir_open_cluster(fat, fat->boot_sector->types.fat_32.root_dir_cluster, ppd);
    }

    fat_priv_dir_info *pd = kmalloc(sizeof(fat_priv_dir_info));
    // *ppd = pd;
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

    klog_debug("fat_open_root_dir(), read %d sectors at LBA %d", 1, fat->root_dir_starting_lba);
    klog_debug_hex(pd->fat16_root_data.sector_buffer, fat->bytes_per_sector, 0);

    file->fs_driver_priv_data = pd;
    return SUCCESS;
}

static int fat_find_entry(file_t *parentdir, char *name, dir_entry_t *entry) {
    fat_info *fat = (fat_info *)parentdir->superblock->priv_fs_driver_data;
    fat_priv_dir_info *pd = (fat_priv_dir_info *)parentdir->fs_driver_priv_data;
    fat_dir_entry *fat_entry = kmalloc(sizeof(fat_dir_entry));
    int err;

    entry->superblock = parentdir->superblock;

    // find the entry in this dir
    fat->ops->priv_dir_seek_slot(fat, pd, 0);
    while (true) {
        err = fat->ops->priv_dir_read_one_entry(fat, pd, fat_entry);
        if (err == ERR_NO_MORE_CONTENT)
            break;
        if (err != SUCCESS)
            goto out;

        klog_debug("fat_readdir(): got entry \"%s\"", fat_entry->short_name);
        if (strcmp(entry->short_name, name) == 0) {
            fat_dir_entry_to_vfs_dir_entry(fat_entry, entry);
            break;
        }
    }

out:
    if (entry != NULL)
        kfree(entry);
    return err;
}



static int fat_opendir(char *path, file_t *file) {
    klog_trace("fat_opendir(\"%s\")", path);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;

    fat_priv_dir_info *pd = NULL;
    int err = fat->ops->priv_dir_find_and_open(fat, path, false, &pd);
    if (err) return err;
    file->fs_driver_priv_data = pd;

    return SUCCESS;
}

static int fat_rewinddir(file_t *file) {
    klog_trace("fat_rewinddir()");
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    fat_priv_dir_info *pd = (fat_priv_dir_info *)file->fs_driver_priv_data;
    
    return fat->ops->priv_dir_seek_slot(fat, pd, 0);
}

static int fat_readdir(file_t *file, struct dir_entry *dir_entry) {
    klog_trace("fat_readdir()");
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    fat_priv_dir_info *pd = (fat_priv_dir_info *)file->fs_driver_priv_data;
    
    fat_dir_entry *fat_entry = kmalloc(sizeof(fat_dir_entry));
    int err = fat->ops->priv_dir_read_one_entry(fat, pd, fat_entry);
    if (err == SUCCESS) {
        klog_debug("fat_readdir(): gotten entry for \"%s\"", fat_entry->short_name);
        fat_dir_entry_to_vfs_dir_entry(fat_entry, dir_entry);
    } else {
        klog_debug("fat_readdir(): priv_dir_read_one_entry() returned %d", err);
    }

    kfree(fat_entry);
    return err;
}

static int fat_closedir(file_t *file) {
    klog_trace("fat_closedir()");
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    fat_priv_dir_info *pd = (fat_priv_dir_info *)file->fs_driver_priv_data;

    int err = fat->ops->priv_dir_close(fat, pd);
    return err;
}

static int fat_mkdir(char *path, file_t *file) {
    klog_trace("fat_mkdir(\"%s\")", path);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    char *name = NULL;
    int err;

    // don't use return, to make sure we release this.
    acquire(&fat_write_lock);
    
    // find and open containing dir 
    fat_priv_dir_info *pd = NULL;
    err = fat->ops->priv_dir_find_and_open(fat, path, true, &pd);
    if (err) goto exit;

    // get name only and create entry
    int path_parts = count_path_parts(path);
    name = kmalloc(strlen(path) + 1);
    get_n_index_path_part(path, path_parts - 1, name);
    err = fat->ops->priv_dir_create_entry(fat, pd, name, true);
    if (err) goto exit;

    err = fat->ops->priv_dir_close(fat, pd);
    if (err) goto exit;

    err = SUCCESS;
exit:
    if (name != NULL)
        kfree(name);
    release(&fat_write_lock);
    return err;
}

static int fat_touch(char *path, file_t *file) {
    klog_trace("fat_touch(\"%s\")", path);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    char *name = NULL;
    int err;

    // don't use return, to make sure we release this.
    acquire(&fat_write_lock);
    
    // find and open containing dir 
    fat_priv_dir_info *pd = NULL;
    err = fat->ops->priv_dir_find_and_open(fat, path, true, &pd);
    if (err) goto exit;

    // get name and invalidate
    int path_parts = count_path_parts(path);
    name = kmalloc(strlen(path) + 1);
    get_n_index_path_part(path, path_parts - 1, name);
    err = fat->ops->priv_dir_create_entry(fat, pd, name, false);
    if (err) goto exit;

    err = fat->ops->priv_dir_close(fat, pd);
    if (err) goto exit;

    err = SUCCESS;
exit:
    if (name != NULL)
        kfree(name);
    release(&fat_write_lock);
    return err;
}

static int fat_unlink(char *path, file_t *file) {
    klog_trace("fat_touch(\"%s\")", path);
    fat_info *fat = (fat_info *)file->partition->fs_driver_priv_data;
    int err;
    char *name = NULL;
    fat_dir_entry *entry = NULL;

    // don't use return, to make sure we release this.
    acquire(&fat_write_lock);
    
    // find and open containing dir 
    fat_priv_dir_info *pd = NULL;
    err = fat->ops->priv_dir_find_and_open(fat, path, true, &pd);
    if (err) goto exit;

    // get filename to find entry
    int path_parts = count_path_parts(path);
    name = kmalloc(strlen(path) + 1);
    entry = kmalloc(sizeof(fat_dir_entry));
    get_n_index_path_part(path, path_parts - 1, name);
    err = fat->ops->find_entry_in_dir(fat, pd, name, entry);
    if (err) goto exit;

    err = fat->ops->priv_dir_entry_invalidate(fat, pd, entry);
    if (err) goto exit;

    err = fat->ops->priv_dir_close(fat, pd);
    if (err) goto exit;

    err = SUCCESS;
exit:
    if (name != NULL)
        kfree(name);
    if (entry != NULL)
        kfree(entry);
    release(&fat_write_lock);
    return err;
}



struct file_ops fat_file_operations = {
    .open_root_dir = fat_open_root_dir
    // .opendir = fat_opendir,
    // .rewinddir = fat_rewinddir,
    // .readdir = fat_readdir,
    // .closedir = fat_closedir,

    // .open = fat_open,
    // .read = fat_read,
    // .write = fat_write,
    // .seek = fat_seek,
    // .close = fat_close,

    // .touch = fat_touch,
    // .mkdir = fat_mkdir,
    // .unlink = fat_unlink
};

static struct file_ops *fat_get_file_operations() {
    return &fat_file_operations;
}
