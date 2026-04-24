#include <ctypes.h>
#include <errors.h>
#include <string.h>
#include <filesys/vfs.h>
#include <devices/storage_dev.h>
#include <memory/kheap.h>

#define max(a, b)    ((a) >= (b) ? (a) : (b))



struct filedisk_info {

    file_t *file;
    size_t size; 

} filedisk_info;

#define SECTOR_SIZE   512



static int filedisk_sector_size(struct storage_dev *dev) {
    return SECTOR_SIZE;
}

static int filedisk_read(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer) {
    if (sector_low * SECTOR_SIZE >= filedisk_info.size)
        return ERR_BAD_ARGUMENT;
    
    vfs_seek(filedisk_info.file, sector_low * SECTOR_SIZE, SEEK_START);
    vfs_read(filedisk_info.file, buffer, SECTOR_SIZE);
    return SUCCESS;
}

static int filedisk_write(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer) {
    if (sector_low * SECTOR_SIZE >= filedisk_info.size)
        return ERR_BAD_ARGUMENT;
    
    vfs_seek(filedisk_info.file, sector_low * SECTOR_SIZE, SEEK_START);
    vfs_write(filedisk_info.file, buffer, SECTOR_SIZE);
    return SUCCESS;
}

// creates a ram disk of some size and registers with device manager
// if instead of memory mapped content, we had a file mapped content,
// we would have invented the loopback device!
void open_filedisk(char *path, size_t size) {
    char *buffer;
    int buffer_size = 64 * 1024;

    buffer = kmalloc(buffer_size);
    memset(buffer, 0, buffer_size);

    vfs_open(path, &filedisk_info.file);
    filedisk_info.size = size;

    int offset = vfs_seek(filedisk_info.file, 0, SEEK_END);
    int to_fill = size - offset;
    while (to_fill > 0) {
        int chunk = max(to_fill, buffer_size);
        vfs_write(filedisk_info.file, buffer, chunk);
        to_fill -= chunk;
    }

    // now we are good
    vfs_seek(filedisk_info.file, 0, SEEK_START);


    struct storage_dev_ops *ops = kmalloc(sizeof(struct storage_dev_ops));
    ops->sector_size = filedisk_sector_size;
    ops->read = filedisk_read;
    ops->write = filedisk_write;

    struct storage_dev *filedisk_dev = kmalloc(sizeof(struct storage_dev));
    memset(filedisk_dev, 0, sizeof(struct storage_dev));
    filedisk_dev->name = "File disk";
    filedisk_dev->ops = ops;
    filedisk_dev->driver_priv_data = &filedisk_info;
    
    register_storage_device(filedisk_dev);
}

void close_filedisk() {
    vfs_close(filedisk_info.file);
}
