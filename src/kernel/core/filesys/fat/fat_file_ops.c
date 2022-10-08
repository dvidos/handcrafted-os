#ifndef _FAT_FILE_OPS_C
#define _FAT_FILE_OPS_C

#include <filesys/vfs.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <klog.h>
#include <errors.h>
#include "fat_priv.h"
#include <lock.h>



static int priv_file_open(fat_info *fat, uint32_t cluster_no, uint32_t file_size, fat_priv_file_info **ppf) {
    klog_trace("priv_file_open(cluster=%d, size=%d)", cluster_no, file_size);

    fat_priv_file_info *pf = kmalloc(sizeof(fat_priv_file_info));
    *ppf = pf;

    memset(pf, 0, sizeof(fat_priv_file_info));
    pf->sector = kmalloc(sizeof(sector_t));
    memset(pf->sector, 0, sizeof(sector_t));
    pf->cluster = kmalloc(sizeof(cluster_t));
    memset(pf->cluster, 0, sizeof(cluster_t));
    pf->sector->buffer = kmalloc(fat->bytes_per_sector);
    memset(pf->sector->buffer, 0, fat->bytes_per_sector);
    pf->cluster->buffer = kmalloc(fat->bytes_per_cluster);
    memset(pf->cluster->buffer, 0, fat->bytes_per_cluster);

    // read the first cluster, to prepare for reading
    if (file_size > 0 && cluster_no > 0) {
        int err = fat->ops->read_data_cluster(fat, cluster_no, pf->cluster);
        if (err) return err;
    }
    
    pf->cluster_n_index = 0;
    pf->first_cluster_no = cluster_no;
    pf->size = file_size;
    pf->offset = 0;

    return SUCCESS;
}

static int priv_file_read(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length) {
    klog_trace("priv_file_read(length=%d)", length);
    int bytes_actually_read = 0;
    int err;

    length = max(length, 0);
    if (length == 0)
        return SUCCESS;
    
    // don't allow reading past EOF
    if (pf->offset + length > pf->size)
        length = pf->size - pf->offset;
    
    while (true) {
        // copy what we can from the current cluster
        int offset_in_cluster = pf->offset - (pf->cluster_n_index * fat->bytes_per_cluster);
        int available_in_cluster = fat->bytes_per_cluster - offset_in_cluster;
        int chunk_len = min(length, available_in_cluster);
        klog_debug("offset in cluster %d, available in cluster %d, chunk len %d", 
            offset_in_cluster,
            available_in_cluster,
            chunk_len
        );
        klog_debug("Copying %d bytes from %p to %p", 
            chunk_len,
            pf->cluster->buffer + offset_in_cluster,
            buffer + bytes_actually_read
        );
        memcpy(buffer + bytes_actually_read, pf->cluster->buffer + offset_in_cluster, chunk_len);
        bytes_actually_read += chunk_len;
        length -= chunk_len;
        pf->offset += chunk_len;
        if (length == 0)
            break;

        if (pf->offset < pf->size && length > 0) {
            // we need to load next cluster
            err = fat->ops->move_to_next_data_cluster(fat, pf, false);
            if (err) return err;
        }
    }

    // instead of success, we return the bytes we actually read
    klog_debug("priv_file_read() done, returning %d", bytes_actually_read);
    return bytes_actually_read;
}

static int priv_file_write(fat_info *fat, fat_priv_file_info *pf, uint8_t *buffer, int length) {
    klog_trace("priv_file_write(length=%d)", length);
    int bytes_actually_written = 0;
    int err = 0;

    length = max(length, 0);
    if (length == 0)
        return SUCCESS;
    
    // ensure there is a cluster to write, even a first one, for new files
    fat->ops->ensure_first_cluster_allocated(fat, pf);

    while (true) {
        // copy what we can into the current cluster
        int offset_in_cluster = pf->offset - (pf->cluster_n_index * fat->bytes_per_cluster);
        int remaining_in_cluster = fat->bytes_per_cluster - offset_in_cluster;
        int chunk_len = min(length, remaining_in_cluster);

        memcpy(pf->cluster->buffer + offset_in_cluster, buffer + bytes_actually_written, chunk_len);
        pf->cluster->dirty = true;
        bytes_actually_written += chunk_len;
        pf->offset += chunk_len;
        pf->size = max(pf->size, pf->offset);

        length -= chunk_len;
        if (length == 0)
            return SUCCESS;

        // move to next cluster, writing this if dirty
        err = fat->ops->move_to_next_data_cluster(fat, pf, true);
        if (err)
            return err;
    }

    return err ? err : bytes_actually_written;
}

static uint32_t calculate_new_file_offset(uint32_t old_position, uint32_t size, int offset, enum seek_origin origin) {
    uint32_t new_position = old_position;
    uint32_t positive_offset = (offset >= 0) ? (uint32_t)offset : (uint32_t)(offset * -1);

    if (origin == SEEK_START) {
        if (offset >= 0)
            new_position = min(positive_offset, size);
        else if (offset < 0)
            new_position = 0;

    } else if (origin == SEEK_END) { 
        if (offset >= 0)
            new_position = size;
        else if (offset < 0)
            new_position = size - min(positive_offset, size);

    } else if (origin == SEEK_CURRENT) {
        if (offset >= 0)
            new_position = min(old_position + offset, size);
        else if (offset < 0)
            new_position = old_position - min(positive_offset, old_position);
    }

    klog_trace("calculate_new_file_offset(ofs=%d, ori=%d, pos=%d, siz=%d) --> %d",
        offset, origin, old_position, size, new_position );
    return new_position;
}

static int priv_file_seek(fat_info *fat, fat_priv_file_info *pf, int offset, enum seek_origin origin) {
    klog_trace("priv_file_seek(offset=%d, origin=%d)", offset, origin);
    int err;

    uint32_t final_position = calculate_new_file_offset(pf->offset, pf->size, offset, origin);
    uint32_t cluster_n_index = final_position / fat->bytes_per_cluster;
    
    err = fat->ops->move_to_n_index_data_cluster(fat, pf, cluster_n_index);
    if (err) return err;

    pf->offset = final_position;
    return (int)final_position;
}

static int priv_file_close(fat_info *fat, fat_priv_file_info *pf) {
    klog_trace("priv_file_close()");

    // possibly write any current cluster if dirty
    if (pf->cluster->cluster_no > 0 && pf->cluster->dirty) {
        int err = fat->ops->write_data_cluster(fat, pf->cluster);
        if (err)
            return err;
    }

    // we must free what we allocated in open()
    kfree(pf->sector->buffer);
    kfree(pf->sector);
    kfree(pf->cluster->buffer);
    kfree(pf->cluster);
    kfree(pf);

    return SUCCESS;
}




#endif