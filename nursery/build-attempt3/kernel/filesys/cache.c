#include <ctypes.h>
#include <lock.h>
#include <drivers/timer.h>
#include <devices/storage_dev.h>
#include <memory/kheap.h>
#include <errors.h>

/*
// functionality of keeping sectors buffers in memory
// in order to improve performance.
// higher layer asks this layer to read/write/sync
// and this layer caches and performs actual storage operations.


#define SUPPORTED_SECTOR_SIZE   512


typedef struct sector_cache {
    bool used;
    bool dirty;
    struct storage_dev *dev;
    int sector_no;
    char buffer[SUPPORTED_SECTOR_SIZE];
    uint64_t last_update_time;
} sector_cache_t;


struct cache_info {
    lock_t write_lock;
    int num_sectors;
    struct sector_cache *sectors_array;
} cache;


static int ensure_entry_in_cache(int dev_no, uint32_t sector_no, int *entry_no);


void init_storage_devices_cache(int number_of_sectors_in_memory) {
    cache.num_sectors = number_of_sectors_in_memory;
    // e.g. 128 sectors * 512 = 64 kbytes
    cache.sectors_array = kmalloc(cache.num_sectors * sizeof(sector_cache_t));
}

void cache_read(int dev_no, uint32_t sector_no, int offset, int length, char *buffer) {

    int entry_no = -1;
    int err = ensure_entry_in_cache(dev_no, sector_no, &entry_no);
    if (err) return err;

    ...
    memcpy(buffer, )
}
void cache_write(int dev_no, uint32_t sector_no, char *buffer, int offset, int length) {
    // if not in cache, read in cache
    // write in cache only
}
void cache_sync(int dev_no, uint32_t sector_no) {
    // write all dirty sectors to disc, ones that are owned by this file handle
}
void cache_sync_all() {
    // write all dirty sectors to disc
    // maybe flush?
}

static int ensure_entry_in_cache(int dev_no, uint32_t sector_no, int *entry_no) {
    int err;
    struct storage_dev *dev;
    struct sector_cache *sc;

    int n = find_cache_entry_fast(dev_no, sector_no);
    if (n >= 0) {
        *entry_no = n;
        return SUCCESS;
    }

    // get the device, we'll need it to load
    struct storage_dev *dev = get_storage_device(dev_no);
    if (dev == NULL)
        return ERR_NO_DEVICE;
    if (dev->ops->sector_size(dev) != SUPPORTED_SECTOR_SIZE)
        return ERR_NOT_SUPPORTED;

    // we need to bring it in, lock as we'll change things
    acquire(cache.write_lock);

    n = find_unused_cache_entry();
    if (n < 0) {
        // evict oldest
        n = get_oldest_entry_to_evict();
        sc = &cache.sectors_array[n];
        if (sc->used && sc->dirty) {
            err = sc->dev->ops->write(sc->dev, sc->sector_no, 0, 1, sc->buffer);
            if (err) goto exit;
        }
        sc->used = false;
    }

    // we can now read into the cache
    sc = &cache.sectors_array[n];
    err = dev->ops->read(dev, sector_no, 0, 1, sc->buffer);
    if (err) goto exit;

    // housekeeping
    sc->dev = dev;
    sc->used = true;
    sc->sector_no = sector_no;
    sc->last_update_time = timer_get_uptime_msecs();
    sc->dirty = false;
    *entry_no = n;

exit:
    release(cache.write_lock);
    return err;
}


static int find_cache_entry_fast(int devno, uint32_t sector_no) {
    // we should have a hash, or a binary tree or something...
    // return -1 if not found
    for (int i = 0; i < cache.num_sectors; i++) {
        if (cache.sectors_array[i].dev == devno && 
            cache.sectors_array[i].sector_no == sector_no)
            return i;
    }
    return -1;
}

static int find_unused_cache_entry() {
    for (int i = 0; i < cache.num_sectors; i++) {
        if (!cache.sectors_array[i].used)
            return i;
    }
    return -1;
}

static int bring_sector_into_cache(int cache_entry, int devno, uint32_t sector_no) {

}
static int get_oldest_entry_to_evict() {

}
static int write_cached_sector_to_disk() {

}


static int load_entry(int n) {

}
static int evict_entry(int n) {

}
static int flush_entry(int n) {

}

*/