#include <devices/storage_dev.h>
#include <filesys/vfs.h>
#include <filesys/partition.h>
#include <filesys/drivers.h>
#include <filesys/mount.h>
#include <memory/kheap.h>
#include <klib/string.h>
#include <klog.h>
#include <errors.h>

MODULE("MOUNT");

static struct mount_info *root_mount_info = NULL;
static struct mount_info *mounts_list = NULL;

static void add_mount_info_to_list(struct mount_info *info);
static void get_desired_root_device(char *kernel_cmd_line, int *dev_no, int *part_no);


struct mount_info *vfs_get_mounts_list() {
    return mounts_list;
}

struct mount_info *vfs_get_root_mount_info() {
    return root_mount_info;
}

struct mount_info *vfs_get_mount_info_by_numbers(int dev_no, int part_no) {
    struct mount_info *p = mounts_list;
    while (p != NULL) {
        if (p->dev->dev_no == dev_no && p->part->part_no == part_no)
            return p;
        p = p->next;
    }
    return NULL;
}

struct mount_info *vfs_get_mount_info_by_path(char *path) {
    struct mount_info *p = mounts_list;
    while (p != NULL) {
        if (strcmp(p->mount_point, path) == 0)
            return p;
        p = p->next;
    }
    return NULL;
}

int vfs_mount(uint8_t dev_no, uint8_t part_no, char *path) {
    klog_trace("vfs_mount(%d, %d, \"%s\")", dev_no, part_no, path);

    struct storage_dev *dev = get_storage_device(dev_no);
    if (dev == NULL) {
        klog_error("Storage dev %d not found - cannot mount root filesystem", dev_no);
        return ERR_NO_DEVICE;
    }
    struct partition *part = get_partition(dev, part_no);
    if (part == NULL) {
        klog_error("Partition %d not found on device %d - cannot mount root filesystem", dev_no, part_no);
        return ERR_NO_PARTITION;
    }
    struct file_system_driver *driver = find_vfs_driver_for_partition(part);
    if (driver == NULL) {
        klog_error("No driver found for dev %d part %d - cannot mount root filesystem", dev_no, part_no);
        return ERR_NO_DRIVER_FOUND;
    }

    struct superblock *sb = kmalloc(sizeof(struct superblock));
    memset(sb, 0, sizeof(struct superblock));
    int err = driver->open_superblock(part, sb);
    if (err) {
        klog_error("Error %d, driver opening superblock", err);
        kfree(sb);
        return err;
    }

    // so now we can mount?
    struct mount_info *mount = kmalloc(sizeof(struct mount_info));
    mount->dev = dev;
    mount->part = part;
    mount->driver = driver;
    mount->superblock = sb;
    mount->mount_point = kmalloc(strlen(path) + 1);
    strcpy(mount->mount_point, path);

    add_mount_info_to_list(mount);
    if (strcmp(mount->mount_point, "/") == 0)
        root_mount_info = mount;

    // we should also open the root directory.
    
    return SUCCESS;
}

static void add_mount_info_to_list(struct mount_info *info) {
    if (mounts_list == NULL) {
        mounts_list = info;
    } else {
        struct mount_info *p = mounts_list;
        while (p->next != NULL)
            p = p->next;
        p->next = info;
    }
    info->next = NULL;
}


int vfs_umount(char *path) {
    klog_trace("vfs_umount(\"%s\")", path);
    // remember to free mount->path as well
    return ERR_NOT_IMPLEMENTED;
}

int vfs_discover_and_mount_filesystems(char *kernel_cmd_line) {

    // first, root device
    int root_dev_no = 0;
    int root_part_no = 0;
    get_desired_root_device(kernel_cmd_line, &root_dev_no, &root_part_no);
    klog_debug("Mounting dev %d, part %d as root. Pass \"root=dnpn\" to kernel to change", root_dev_no, root_part_no);

    int err = vfs_mount(root_dev_no, root_part_no, "/");
    if (err)
        return err;

    // in theory we could parse /etc/mtab to mount the rest of the files
    // but we are not at this mature level yet
    // plus, maybe this should happen in user land, using the init procedure.
    // so, let's mount the remaining partitions
    
    klog_debug("Trying to mount remaining partitions");
    struct partition *part = get_partitions_list();
    while (part != NULL) {
        int dev_no = part->dev->dev_no;
        if (vfs_get_mount_info_by_numbers(dev_no, part->part_no) != NULL) {
            // already mounted
            part = part->next;
            continue;
        }
        struct file_system_driver *drv = find_vfs_driver_for_partition(part);
        if (drv == NULL) {
            klog_debug("No filesystem driver for dev %d, part %d, maybe write one?", dev_no, part->part_no);
            part = part->next;
            continue;
        }

        char path[16];
        sprintfn(path, sizeof(path), "/mnt/d%dp%d", part->dev->dev_no, part->part_no);
        // in theory we could create the folders needed to mount
        vfs_mount(part->dev->dev_no, part->part_no, path);

        part = part->next;
    }

    return SUCCESS;
}


static void get_desired_root_device(char *kernel_cmd_line, int *dev_no, int *part_no) {
    // a root device can be designated in the kernel cmdline.
    // the expected format is "root=d<num>p<num>" e.g. "d2p5"
    // it stands for "disk number", "partition number"
    // very quick and dirty, single digits supported for now.
    *dev_no = 1;
    *part_no = 1;

    if (kernel_cmd_line != NULL && *kernel_cmd_line != '\0') {
        klog_debug("Parsing kernel cmdline (\"%s\") for root", kernel_cmd_line);
        char *p = strstr(kernel_cmd_line, "root=");
        if (p != NULL && p[5] == 'd' && p[7] == 'p') {
            *dev_no = (p[6] - '0');
            *part_no = (p[8] - '0');
        }
    }
}
