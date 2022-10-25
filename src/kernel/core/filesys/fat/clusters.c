#include "fat_priv.h"
#include <errors.h>
#include <klog.h>



static int read_allocation_table_sector(fat_info *fat, uint32_t sector_no, sector_t *sector) {
    int err = fat->partition->dev->ops->read(fat->partition->dev,
        fat->fat_starting_lba + sector_no, 0,
        1, sector->buffer
    );
    if (err == 0) {
        sector->sector_no = sector_no;
        sector->dirty = false;
        sector->loaded = true;
    }
    klog_trace("read_allocation_table_sector(sector=%d) -> %d", sector_no, err);
    return err;
}

static int write_allocation_table_sector(fat_info *fat, sector_t *sector) {
    int err = fat->partition->dev->ops->write(fat->partition->dev,
        fat->fat_starting_lba + sector->sector_no, 0,
        1, sector->buffer
    );
    klog_trace("write_allocation_table_sector(sector=%d) -> %d", sector->sector_no, err);
    if (err == 0) {
        sector->dirty = false;
    }
    return err;
}

static int get_allocation_table_entry(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t *value) {
    uint32_t offset_in_fat;
    if (fat->fat_type == FAT12) {
        // multiply by 1.5 <==> 1 + (1/2), (rounding down err correction later)
        offset_in_fat = cluster_no + (cluster_no / 2);
    } else if (fat->fat_type == FAT16) {
        offset_in_fat = cluster_no * 2; // two bytes for 16 bits
    } else if (fat->fat_type == FAT32) {
        offset_in_fat = cluster_no * 4; // four bytes for 32 bits
    } else {
        return ERR_NOT_SUPPORTED;
    }

    // find the sector of FAT to load and load it.
    uint32_t fat_sector_no    = offset_in_fat / fat->bytes_per_sector;
    uint32_t offset_in_sector = offset_in_fat % fat->bytes_per_sector;

    bool already_loaded = sector->loaded && sector->sector_no == fat_sector_no;
    if (!already_loaded) {
        int err = read_allocation_table_sector(fat, fat_sector_no, sector);
        if (err)
            return err;
        sector->sector_no = fat_sector_no;
        sector->loaded = true;

        klog_debug("allocation table sector No %d, contents follow, our offset is %d (0x%x)", fat_sector_no, offset_in_sector, offset_in_sector);
        klog_debug_hex(sector->buffer, fat->bytes_per_sector, 0);
    }

    // now that we have the sector, extract the next cluster_no
    if (fat->fat_type == FAT12) {
        uint16_t word = *(uint16_t *)(sector->buffer + offset_in_sector);

        // see if we rounded down (when we divided by two)
        if (cluster_no & 0x0001)
            word >>= 4;    // use three higher nibbles (out of the four)
        else
            word &= 0xFFF; // use three lower nibbles (out of the four)

        *value = word;
    } else if (fat->fat_type == FAT16) {
        *value = *(uint16_t *)(sector->buffer + offset_in_sector);

    } else if (fat->fat_type == FAT32) {
        *value = 
            (*(uint32_t *)(sector->buffer + offset_in_sector))
            & 0x0FFFFFFF; // ignore four upper bits

    } else {
        return ERR_NOT_SUPPORTED;
    }

    klog_trace("get_allocation_table_entry(cluster=%d), new_value=%d", cluster_no, *value);
    return SUCCESS;
}

static int set_allocation_table_entry(fat_info *fat, sector_t *sector, uint32_t cluster_no, uint32_t value) {
    klog_trace("set_allocation_table_entry(cluster=%d, value=%d)", cluster_no, value);
    uint32_t offset_in_fat;
    if (fat->fat_type == FAT12) {
        // multiply by 1.5 <==> 1 + (1/2), (rounding down err correction later)
        offset_in_fat = cluster_no + (cluster_no / 2);
    } else if (fat->fat_type == FAT16) {
        offset_in_fat = cluster_no * 2; // two bytes for 16 bits
    } else if (fat->fat_type == FAT32) {
        offset_in_fat = cluster_no * 4; // four bytes for 32 bits
    } else {
        return ERR_NOT_SUPPORTED;
    }

    // find the sector of FAT to load and update it.
    uint32_t fat_sector_no    = offset_in_fat / fat->bytes_per_sector;
    uint32_t offset_in_sector = offset_in_fat % fat->bytes_per_sector;

    bool already_loaded = sector->loaded && sector->sector_no == fat_sector_no;
    if (!already_loaded) {
        int err = read_allocation_table_sector(fat, fat_sector_no, sector);
        if (err)
            return err;
        sector->sector_no = fat_sector_no;
        sector->loaded = true;
    }

    // now that we have the sector, set the value
    if (fat->fat_type == FAT12) {
        uint16_t word = *(uint16_t *)(sector->buffer + offset_in_sector);

        // see if we rounded down (when we divided by two)
        if (cluster_no & 0x0001)
            word = (word & 0x000F) | ((value & 0xFFF) << 4);    // use three higher nibbles (out of the four)
        else
            word = (word & 0xF000) | (value & 0xFFF); // use three lower nibbles (out of the four)

        *(uint16_t *)(sector->buffer + offset_in_sector) = word;
    } else if (fat->fat_type == FAT16) {
        *(uint16_t *)(sector->buffer + offset_in_sector) = value;

    } else if (fat->fat_type == FAT32) {
        // clear four upper bits
        *(uint32_t *)(sector->buffer + offset_in_sector) = value & 0x0FFFFFFF;

    } else {
        return ERR_NOT_SUPPORTED;
    }

    sector->dirty = true;
    return SUCCESS;
}

static bool is_end_of_chain_entry_value(fat_info *fat, uint32_t value) {
    return value == fat->end_of_chain_value || value > fat->largest_cluster_no;
}

static int find_a_free_cluster(fat_info *fat, sector_t *sector, uint32_t *cluster_no) {
    klog_trace("find_a_free_cluster()");
    uint32_t value;
    for (uint32_t cl = 2; cl < fat->largest_cluster_no; cl++) {
        int err = get_allocation_table_entry(fat, sector, cl, &value);
        if (err)
            return err;
        if (value == 0) {
            *cluster_no = cl;
            klog_trace("find_a_free_cluster() -> free_cluster=%d)", *cluster_no);
            return SUCCESS;
        }
    }

    return ERR_NO_SPACE_LEFT;
}

static int get_n_index_cluster_no(fat_info *fat, sector_t *sector, uint32_t first_cluster, uint32_t cluster_n_index, uint32_t *cluster_no) {
    klog_trace("get_n_index_cluster_no(cluster_n_index=%d)", cluster_n_index);

    uint32_t curr_cluster_no = first_cluster;
    uint32_t value = 0;
    while (cluster_n_index-- > 0) {
        int err = get_allocation_table_entry(fat, sector, curr_cluster_no, &value);
        if (err)
            return err;
        curr_cluster_no = value;
    }

    *cluster_no = curr_cluster_no;
    return SUCCESS;
}

static int read_data_cluster(fat_info *fat, uint32_t cluster_no, cluster_t *cluster) {
    int err = fat->partition->dev->ops->read(
        fat->partition->dev,
        fat->data_clusters_starting_lba + ((cluster_no - 2) * fat->sectors_per_cluster),
        0,
        fat->sectors_per_cluster,
        cluster->buffer
    );
    if (err == 0) {
        cluster->cluster_no = cluster_no;
        cluster->dirty = false;
    }
    klog_trace("read_data_cluster(cluster=%d) --> %d", cluster_no, err);
    // klog_debug_hex(cluster->buffer, fat->bytes_per_cluster, 0);
    return err;
}

static int write_data_cluster(fat_info *fat, cluster_t *cluster) {
    int err = fat->partition->dev->ops->write(
        fat->partition->dev,
        fat->data_clusters_starting_lba + ((cluster->cluster_no - 2) * fat->sectors_per_cluster),
        0,
        fat->sectors_per_cluster,
        cluster->buffer
    );
    if (err == 0) {
        cluster->dirty = false;
    }
    klog_trace("write_data_cluster(cluster=%d) --> %d", cluster->cluster_no, err);
    return err;
}

// make sure a file is allocated for writing, used for new files
static int ensure_first_cluster_allocated(fat_info *fat, fat_priv_file_info *pf) {
    int err;

    if (pf->first_cluster_no > 0)
        return SUCCESS;

    // we see need to allocate. assuming caller has locked.
    // find a free cluster to allocate for this file
    uint32_t new_cluster_no;
    err = find_a_free_cluster(fat, pf->sector, &new_cluster_no);
    if (err) return err;
    
    // but also mark the new one as end-of-chain, i.e. non-free
    err = set_allocation_table_entry(fat, pf->sector, new_cluster_no, fat->end_of_chain_value);
    if (err) return err;
    err = write_allocation_table_sector(fat, pf->sector);
    if (err) return err;

    // clear this sector
    memset(pf->cluster->buffer, 0, fat->bytes_per_cluster);
    pf->cluster->cluster_no = new_cluster_no;
    pf->cluster->dirty = false;
    pf->cluster_n_index = 0;

    // TODO: this needs to be persisted in the directory entry...
    pf->first_cluster_no = new_cluster_no;
    return SUCCESS;
}

// reads next data cluster, creating it if needed. pf->cluster assumed to contain current cluster
static int move_to_next_data_cluster(fat_info *fat, fat_priv_file_info *pf, bool create_if_needed) {
    int err;

    // before losing contents, persist current cluster if dirty
    if (pf->cluster->dirty) {
        err = write_data_cluster(fat, pf->cluster);
        if (err) return err;
    }

    uint32_t next_cluster_no;
    err = get_allocation_table_entry(fat, pf->sector, pf->cluster->cluster_no, &next_cluster_no);
    if (err) return err;

    if (!is_end_of_chain_entry_value(fat, next_cluster_no)) {
        err = read_data_cluster(fat, next_cluster_no, pf->cluster);
        if (err) return err;

        pf->cluster_n_index++;

    } else if (create_if_needed) {

        // find a free cluster to allocate for this file
        uint32_t new_cluster_no;
        err = find_a_free_cluster(fat, pf->sector, &new_cluster_no);
        if (err) return err;
        
        // we must write the pointer from the previous cluster to this one
        err = set_allocation_table_entry(fat, pf->sector, pf->cluster->cluster_no, new_cluster_no);
        if (err) return err;
        err = write_allocation_table_sector(fat, pf->sector);
        if (err) return err;
        
        // but also mark the new one as end-of-chain, i.e. non-free
        err = set_allocation_table_entry(fat, pf->sector, new_cluster_no, fat->end_of_chain_value);
        if (err) return err;
        err = write_allocation_table_sector(fat, pf->sector);
        if (err) return err;

        memset(pf->cluster->buffer, 0, fat->bytes_per_cluster);
        pf->cluster->cluster_no = new_cluster_no;
        pf->cluster->dirty = false;
        pf->cluster_n_index++;

    } else {

        // we found end of cluster chain and we cannot create
        return ERR_NO_MORE_CONTENT;
    }

    return SUCCESS;
}

// ensures the n'th cluster of the file is loaded
static int move_to_n_index_data_cluster(fat_info *fat, fat_priv_file_info *pf, uint32_t cluster_n_index) {
    int err;

    // maybe we don't need to do anything
    if (cluster_n_index == pf->cluster_n_index)
        return SUCCESS;
    
    // discover the cluster to load
    uint32_t target_cluster_no;
    err = get_n_index_cluster_no(fat, pf->sector, pf->first_cluster_no, cluster_n_index, &target_cluster_no);
    if (err) return err;

    // make sure we write our cluster first
    if (pf->cluster->dirty) {
        err = write_data_cluster(fat, pf->cluster);
        if (err) return err;
    }

    // now load the cluster
    err = read_data_cluster(fat, target_cluster_no, pf->cluster);
    if (err) return err;

    pf->cluster_n_index = cluster_n_index;
    return SUCCESS;
}

static int allocate_new_cluster_chain(fat_info *fat, sector_t *sector, cluster_t *cluster, bool clear_data, uint32_t *first_cluster_no) {

    uint32_t cluster_no;
    int err = find_a_free_cluster(fat, sector, &cluster_no);
    if (err) return err;
    
    // but also mark the new one as end-of-chain, i.e. non-free
    err = set_allocation_table_entry(fat, sector, cluster_no, fat->end_of_chain_value);
    if (err) return err;

    err = write_allocation_table_sector(fat, sector);
    if (err) return err;

    if (clear_data) {
klog_debug("Clearing cluster data, for cluster %u", cluster_no);
        cluster->cluster_no = cluster_no;
        err = fat->ops->read_data_cluster(fat, cluster_no, cluster);
        if (err) return err;
        cluster->cluster_no = cluster_no;
        cluster->dirty = true;
        memset(cluster->buffer, 0, fat->bytes_per_cluster);
        err = fat->ops->write_data_cluster(fat, cluster);
        if (err) return err;
    }

    *first_cluster_no = cluster_no;
klog_debug("allocate_new_cluster_chain() -> %d", SUCCESS);
    return SUCCESS;
}

static int release_allocation_chain(fat_info *fat, sector_t *sector, uint32_t first_cluster_no) {
    uint32_t cluster = 0;
    uint32_t value = 0;
    int err;

    cluster = first_cluster_no;
    while (true) {
        err = fat->ops->get_allocation_table_entry(fat, sector, cluster, &value);
        if (err) break;

        err = fat->ops->set_allocation_table_entry(fat, sector, cluster, 0);
        if (err) break;

        // are we done?
        if (fat->ops->is_end_of_chain_entry_value(fat, value) || value == 0)
            break;
        
        // use value as the next cluster to clear
        cluster = value;
    }

    return err;
}


