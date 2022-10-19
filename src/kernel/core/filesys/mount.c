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
static void remove_mount_info_from_list(struct mount_info *info);
static void get_desired_root_device(char *kernel_cmd_line, int *dev_no, int *part_no);



struct mount_info *vfs_get_mounts_list() {
    return mounts_list;
}

struct mount_info *vfs_get_root_mount() {
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
    struct filesys_driver *driver = find_vfs_driver_for_partition(part);
    if (driver == NULL) {
        klog_error("No driver found for dev %d part %d - cannot mount root filesystem", dev_no, part_no);
        return ERR_NO_DRIVER_FOUND;
    }

    int err;
    struct superblock *sb = NULL;
    struct mount_info *mount = NULL;

    // each mounted filesystem has this structure populated by the fs driver
    sb = kmalloc(sizeof(struct superblock));
    sb->driver = driver;
    memset(sb, 0, sizeof(struct superblock));
    err = driver->open_superblock(part, sb);
    if (err) {
        klog_error("Error %d, driver opening superblock", err);
        goto error;
    }

    // so now we can mount?
    mount = kmalloc(sizeof(struct mount_info));
    mount->dev = dev;
    mount->part = part;
    mount->driver = driver;
    mount->superblock = sb;
    mount->mount_point = kmalloc(strlen(path) + 1);
    strcpy(mount->mount_point, path);

    // we should get the two file descriptors...

    if (strcmp(path, "/") == 0) {
        // we are mounting the root file system
        mount->host_dir = NULL;
    } else {
        // TODO: we are mounting somewhere we should discover it.
        klog_warn("Must resolve where we mount");
    }

    // we also keep the hosted root directory
    if (mount->superblock->ops->root_dir_descriptor == NULL) {
        klog_error("Driver %s does not support root_dir_descriptor() method", driver->name);
        err = ERR_NOT_SUPPORTED;
        goto error;
    }
    err = mount->superblock->ops->root_dir_descriptor(mount->superblock, &mount->mounted_fs_root);
    if (err) {
        klog_error("Error %d retrieving root dir descriptor", err);
        goto error;
    } 

    // we also open the root directory (in order to resolve any path names)
    if (mount->superblock->ops->deprecated_open_root_dir == NULL) {
        klog_error("Driver %s does not support deprecated_open_root_dir() method", driver->name);
        err = ERR_NOT_SUPPORTED;
        goto error;
    }
    mount->root_dir = kmalloc(sizeof(file_t));
    err = mount->superblock->ops->deprecated_open_root_dir(mount->superblock, mount->root_dir);
    if (err) {
        klog_error("Error %d when opening root dir", err);
        goto error;
    }

    // add to list, keep as root
    add_mount_info_to_list(mount);
    if (strcmp(mount->mount_point, "/") == 0)
        root_mount_info = mount;

    klog_info("Device %d partition %d, driver \"%s\", now mounted on \"%s\"",
        mount->dev->dev_no,
        mount->part->part_no,
        mount->driver->name,
        mount->mount_point);
    return SUCCESS;
error:
    if (sb != NULL) kfree(sb);
    if (mount != NULL) {
        if (mount->mount_point != NULL) kfree(mount->mount_point);
        if (mount->root_dir != NULL) kfree(mount->root_dir);
        kfree(mount);
    }
    return err;
}

int vfs_umount(char *path) {
    klog_trace("vfs_umount(\"%s\")", path);
    int err;

    mount_info_t *mount = vfs_get_mount_info_by_path(path);
    if (mount == NULL)
        return ERR_NOT_FOUND;

    if (mount->root_dir != NULL) {
        err = mount->superblock->ops->closedir(mount->root_dir);
        if (err) return err;
    }

    err = mount->driver->close_superblock(mount->superblock);
    if (err) return err;

    remove_mount_info_from_list(mount);
    if (strcmp(mount->mount_point, "/") == 0)
        root_mount_info = NULL;


    if (mount->root_dir != NULL) kfree(mount->root_dir);
    kfree(mount->superblock);
    kfree(mount->mount_point);
    kfree(mount);

    return SUCCESS;
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
        struct filesys_driver *drv = find_vfs_driver_for_partition(part);
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

static void remove_mount_info_from_list(struct mount_info *info) {
    if (mounts_list == info) {
        mounts_list = info->next;
    } else {
        struct mount_info *p = mounts_list->next;
        while (p->next != NULL && p->next != info)
            p = p->next;
        if (p->next == info)
            p->next = info->next;
    }
    info->next = NULL;
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
