#include <errors.h>
#include <klib/string.h>
#include "fat_priv.h"
#include "fat_dir_ops.c"
#include "fat_file_ops.c"



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

    fat->ops->read_allocation_table_sector = read_allocation_table_sector;
    fat->ops->write_allocation_table_sector = write_allocation_table_sector;
    fat->ops->get_allocation_table_entry = get_allocation_table_entry;
    fat->ops->set_allocation_table_entry = set_allocation_table_entry;
    fat->ops->is_end_of_chain_entry_value = is_end_of_chain_entry_value;
    fat->ops->find_a_free_cluster = find_a_free_cluster;
    fat->ops->get_n_index_cluster_no = get_n_index_cluster_no;
    fat->ops->allocate_new_cluster_chain = allocate_new_cluster_chain;
    fat->ops->release_allocation_chain = release_allocation_chain;
    
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
    
    fat->root_dir_descriptor = create_file_descriptor(
        superblock, 
        "/", 
        fat->fat_type == FAT32 ? fat->boot_sector->types.fat_32.root_dir_cluster : 0,
        NULL
    );
    fat->root_dir_descriptor->flags = FD_DIR;

    fat->io_buffers = kmalloc(sizeof(struct io_buffers));
    fat->io_buffers->sector = kmalloc(sizeof(sector_t));
    memset(fat->io_buffers->sector, 0, sizeof(sector_t));
    fat->io_buffers->sector->buffer = kmalloc(fat->bytes_per_sector);
    memset(fat->io_buffers->sector->buffer, 0, fat->bytes_per_sector);
    fat->io_buffers->cluster = kmalloc(sizeof(cluster_t));
    memset(fat->io_buffers->cluster, 0, sizeof(cluster_t));
    fat->io_buffers->cluster->buffer = kmalloc(fat->bytes_per_cluster);
    memset(fat->io_buffers->cluster->buffer, 0, fat->bytes_per_cluster);
    
    debug_fat_info(fat);

    superblock->partition = partition;
    superblock->ops = fat_get_file_operations();
    superblock->priv_fs_driver_data = fat;

    return SUCCESS;

error:
    if (boot_sector != NULL) kfree(boot_sector);
    if (fat->root_dir_descriptor != NULL) destroy_file_descriptor(fat->root_dir_descriptor);
    if (fat->ops != NULL) kfree(fat->ops);
    if (fat->io_buffers != NULL) {
        if (fat->io_buffers->sector != NULL) {
            if (fat->io_buffers->sector->buffer != NULL)
                kfree(fat->io_buffers->sector->buffer);
            kfree(fat->io_buffers->sector);
        }
        if (fat->io_buffers->cluster != NULL) {
            if (fat->io_buffers->cluster->buffer != NULL)
                kfree(fat->io_buffers->cluster->buffer);
            kfree(fat->io_buffers->cluster);
        }
        kfree(fat->io_buffers);
    }
    if (fat != NULL) kfree(fat);
    return err;
}

static int fat_close_superblock(struct superblock *superblock) {
    klog_trace("fat_close_superblock()");

    // ideally we'll sync all buffers to disk first, then apply write lock

    fat_info *fat = (fat_info *)superblock->priv_fs_driver_data;

    if (fat->root_dir_descriptor != NULL)
        destroy_file_descriptor(fat->root_dir_descriptor);
    
    kfree(fat->ops);
    kfree(fat);

    return SUCCESS;
}

static int fat_lookup(file_descriptor_t *dir, char *name, file_descriptor_t **result) {
    klog_trace("fat_lookup(dir=0x%x, name=\"%s\")", dir, name);

    // we must find the entry "name" in directory dir, return pointer to result.
    // something analogous to opendir()/readdir()/closedir().
    fat_info *fat = (fat_info *)dir->superblock->priv_fs_driver_data;
    int err = ERR_NOT_SUPPORTED;
    fat_priv_dir_info *pdi;
    fat_dir_entry *entry = kmalloc(sizeof(fat_dir_entry));

    if ((fat->fat_type == FAT12 || fat->fat_type == FAT16) && dir->location == 0)
        err = fat->ops->priv_dir_open_root(fat, &pdi);
    else
        err = fat->ops->priv_dir_open_cluster(fat, dir->location, &pdi);
    if (err) goto out;

    err = fat->ops->find_entry_in_dir(fat, pdi, name, entry);
    if (err) goto close;

    fat_dir_entry_to_file_descriptor(dir, entry, result);
    if (err) goto close;

close:
    // don't affect `err`, we may have some specific error
    fat->ops->priv_dir_close(fat, pdi);
out:
    if (entry)
        kfree(entry);
    return err;
}

static int fat_open(file_descriptor_t *fd, int flags, file_t **file) {
    klog_trace("fat_open2(descriptor=0x%x)", fd);
    fat_info *fat = (fat_info *)fd->superblock->priv_fs_driver_data;

    if ((fd->flags & FD_FILE) == 0)
        return ERR_NOT_A_FILE;
    
    fat_priv_file_info *pfi = NULL;
    int err = fat->ops->priv_file_open(fat, fd->location, fd->size, &pfi);
    if (err)
        return err;

    (*file) = create_file_t(fd->superblock, fd);
    (*file)->fs_driver_private_data = pfi;
    return SUCCESS;
}

static int fat_read(file_t *file, char *buffer, int length) {
    klog_trace("fat_read(length=%d)", length);
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_file_info *pfi = (fat_priv_file_info *)file->fs_driver_private_data;
    return fat->ops->priv_file_read(fat, pfi, buffer, length);
}

static int fat_write(file_t *file, char *buffer, int length) {
    klog_trace("fat_write(length=%d)", length);
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_file_info *pfi = (fat_priv_file_info *)file->fs_driver_private_data;
    acquire(&file->superblock->write_lock);
    int err = fat->ops->priv_file_write(fat, pfi, buffer, length);
    release(&file->superblock->write_lock);
    return err;
}

static int fat_seek(file_t *file, int offset, enum seek_origin origin) {
    klog_trace("fat_seek(offset=%d, origin=%d)", offset, origin);
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_file_info *pfi = (fat_priv_file_info *)file->fs_driver_private_data;
    return fat->ops->priv_file_seek(fat, pfi, offset, origin);
}

static int fat_flush(file_t *file) {
    klog_trace("fat_flush(file=0x%x)", file);

    int err;
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_file_info *pfi = (fat_priv_file_info *)file->fs_driver_private_data;

    if (pfi->cluster->dirty) {
        err = fat->ops->write_data_cluster(fat, pfi->cluster);
        if (err) return err;
    }

    if (pfi->sector->dirty) {
        err = fat->ops->write_allocation_table_sector(fat, pfi->sector);
        if (err) return err;
    }

    return SUCCESS;
}

static int fat_close(file_t *file) {
    klog_trace("fat_close()");
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_file_info *pfi = (fat_priv_file_info *)file->fs_driver_private_data;
    return fat->ops->priv_file_close(fat, pfi);
}


static int fat_root_descriptor(superblock_t *superblock, file_descriptor_t **fd) {
    fat_info *fat = (fat_info *)superblock->priv_fs_driver_data;
    *fd = fat->root_dir_descriptor;
    return SUCCESS;
}


static int fat_opendir(file_descriptor_t *fd, file_t **dir) {
    klog_trace("fat_opendir()");

    fat_info *fat = (fat_info *)fd->superblock->priv_fs_driver_data;
    int err;
    fat_priv_dir_info *pdi;

    if ((fat->fat_type == FAT12 || fat->fat_type == FAT16) && fd->location == 0)
        err = fat->ops->priv_dir_open_root(fat, &pdi);
    else
        err = fat->ops->priv_dir_open_cluster(fat, fd->location, &pdi);
    if (err) goto out;

    (*dir) = create_file_t(fd->superblock, fd);
    (*dir)->fs_driver_private_data = pdi;

out:
    return err;
}

static int fat_rewinddir(file_t *file) {
    klog_trace("fat_rewinddir()");
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_dir_info *pd = (fat_priv_dir_info *)file->fs_driver_private_data;
    
    return fat->ops->priv_dir_seek_slot(fat, pd, 0);
}

static int fat_readdir(file_t *file, file_descriptor_t **fd) {
    klog_trace("fat_readdir()");
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_dir_info *pdi = (fat_priv_dir_info *)file->fs_driver_private_data;
    
    fat_dir_entry *fat_entry = kmalloc(sizeof(fat_dir_entry));
    int err = fat->ops->priv_dir_read_one_entry(fat, pdi, fat_entry);
    if (err == SUCCESS) {
        klog_debug("fat_readdir(): gotten entry for \"%s\"", fat_entry->short_name);
        fat_dir_entry_to_file_descriptor(file->descriptor, fat_entry, fd);
    } else {
        klog_debug("fat_readdir(): priv_dir_read_one_entry() returned %d", err);
    }

    kfree(fat_entry);
    return err;
}

static int fat_closedir(file_t *file) {
    klog_trace("fat_closedir()");
    fat_info *fat = (fat_info *)file->superblock->priv_fs_driver_data;
    fat_priv_dir_info *pd = (fat_priv_dir_info *)file->fs_driver_private_data;

    int err = fat->ops->priv_dir_close(fat, pd);
    return err;
}


static int create_directory_entry(file_descriptor_t *parent_dir, char *name, uint32_t cluster_no, uint32_t size, bool allocate_cluster, bool want_directory) {
    klog_trace("create_directory_entry(parent=%s, name=%s, clust=%u, size=%u, allocate=%d, want_dir=%d)", parent_dir->name, name, cluster_no, size, allocate_cluster, want_directory);
    fat_info *fat = (fat_info *)parent_dir->superblock->priv_fs_driver_data;
    int err;

    // find and open containing dir 
    fat_priv_dir_info *pdi = NULL;
    if ((fat->fat_type == FAT12 || fat->fat_type == FAT16) && parent_dir->location == 0)
        err = fat->ops->priv_dir_open_root(fat, &pdi);
    else
        err = fat->ops->priv_dir_open_cluster(fat, parent_dir->location, &pdi);
    if (err) goto exit;

    // see if it exists
    fat_dir_entry entry;
    err = fat->ops->find_entry_in_dir(fat, pdi, name, &entry);
    if (err == 0) {
        err = ERR_ALREADY_EXISTS;
        goto exit;
    }

    // if the caller wants a directory, there's two options:
    // - they want to create the "." or ".." entries, who have a specific cluster no.
    // - they want to create a new generic directory, which needs to point to a cluster.
    // remember, a directory pointing to zero cluster is essentially pointing to the root directory...
    // the "." will point to a valid cluster, the ".." may point to a zero cluster, in a FAT16, firts level directory!
    // so we cannot create a dir entry without a cluster, 
    // cause if we do, we'll always open the root directory when we open it!
     
    if (want_directory && allocate_cluster) {
        err = fat->ops->allocate_new_cluster_chain(fat, fat->io_buffers->sector, fat->io_buffers->cluster, true, &cluster_no);
        if (err) {
            klog_warn("Failed allocating new cluster for new directory...");
            goto exit;
        }
        size = fat->bytes_per_cluster;
    }

    err = fat->ops->priv_dir_create_entry(fat, pdi, name, cluster_no, size, want_directory);
    if (err) goto exit;

    err = SUCCESS;
exit:
    if (pdi != NULL)
        fat->ops->priv_dir_close(fat, pdi);

    return err;
}

static int remove_directory_entry(file_descriptor_t *parent_dir, char *name, bool want_directory) {
    klog_trace("remove_directory_entry(parent=%s, name=%s, want_dir=%d)", parent_dir->name, name, want_directory);
    fat_info *fat = (fat_info *)parent_dir->superblock->priv_fs_driver_data;
    int err;

    // find and open containing dir 
    fat_priv_dir_info *pdi = NULL;
    if ((fat->fat_type == FAT12 || fat->fat_type == FAT16) && parent_dir->location == 0)
        err = fat->ops->priv_dir_open_root(fat, &pdi);
    else
        err = fat->ops->priv_dir_open_cluster(fat, parent_dir->location, &pdi);
    if (err) goto exit;

    // see if it exists
    fat_dir_entry entry;
    err = fat->ops->find_entry_in_dir(fat, pdi, name, &entry);
    if (err == ERR_NO_MORE_CONTENT) {
        err = ERR_NOT_FOUND;
        goto exit;
    }
    if (err) goto exit;

    // make sure it's the correct type
    if (want_directory && !entry.attributes.flags.directory) {
        err = ERR_NOT_A_DIRECTORY;
        goto exit;
    } else if (!want_directory && entry.attributes.flags.directory) {
        err = ERR_NOT_A_FILE;
        goto exit;
    }

    // mark the entry as invalid
    err = fat->ops->priv_dir_entry_invalidate(fat, pdi, &entry);
    if (err) goto exit;

    // mark all clusters as available (free file space)
    err = fat->ops->release_allocation_chain(fat, pdi->pf->sector, entry.first_cluster_no);
    if (err) goto exit;

    err = SUCCESS;
exit:
    if (pdi != NULL)
        fat->ops->priv_dir_close(fat, pdi);
    release(&parent_dir->superblock->write_lock);
    return err;
}




static int fat_touch(file_descriptor_t *parent_dir, char *name) {
    klog_trace("fat_touch(\"%s\")", name);
    acquire(&parent_dir->superblock->write_lock);

    // a zero sized file does not need cluster allocated.
    int err = create_directory_entry(parent_dir, name, 0, 0, false, false);

    release(&parent_dir->superblock->write_lock);
    return err;
}

static int fat_unlink(file_descriptor_t *parent_dir, char *name) {
    klog_trace("fat_touch(\"%s\")", name);
    acquire(&parent_dir->superblock->write_lock);

    int err = remove_directory_entry(parent_dir, name, false);

    release(&parent_dir->superblock->write_lock);
    return err;
}

static int fat_mkdir(file_descriptor_t *parent_dir, char *name) {
    klog_trace("fat_mkdir(\"%s\")", name);
    acquire(&parent_dir->superblock->write_lock);

    // if we created a directory, we need to create the "." and ".." entries
    // essentially, we are creating three directory entries, all of type directory!
    file_descriptor_t *new_dir = NULL;

    int err = create_directory_entry(parent_dir, name, 0, 0, true, true);
    if (err) goto exit;

    err = fat_lookup(parent_dir, name, &new_dir);
    if (err) goto exit;

    klog_debug("Newly created dir descriptor:");
    debug_file_descriptor(new_dir, 0);
    
    err = create_directory_entry(new_dir, ".", new_dir->location, new_dir->size, false, true);
    if (err) goto exit;
    err = create_directory_entry(new_dir, "..", parent_dir->location, 0, false, true);
    if (err) goto exit;

exit:
    if (new_dir != NULL)
        destroy_file_descriptor(new_dir);

    release(&parent_dir->superblock->write_lock);
    return err;
}

static int fat_rmdir(file_descriptor_t *parent_dir, char *name) {
    klog_trace("fat_rmdir(\"%s\")", name);
    acquire(&parent_dir->superblock->write_lock);

    int err = remove_directory_entry(parent_dir, name, true);

    release(&parent_dir->superblock->write_lock);
    return err;
}


struct file_ops fat_file_operations = {
    .root_dir_descriptor = fat_root_descriptor,
    .lookup = fat_lookup,

    .open = fat_open,
    .read = fat_read,
    .write = fat_write,
    .seek = fat_seek,
    .flush = fat_flush,
    .close = fat_close,

    .opendir = fat_opendir,
    .rewinddir = fat_rewinddir,
    .readdir = fat_readdir,
    .closedir = fat_closedir,

    .touch = fat_touch,
    .unlink = fat_unlink,
    .mkdir = fat_mkdir,
    .rmdir = fat_rmdir,
};

static struct file_ops *fat_get_file_operations() {
    return &fat_file_operations;
}
