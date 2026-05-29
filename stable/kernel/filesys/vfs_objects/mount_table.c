#include "mount_table.h"
#include "../../utils/assert.h"
#include "../../memory/kheap.h"
#include "../../include/uapi/errors.h"


static int mount_table_count_entries(mount_table_t *mt) {
    int count = 0;
    for (mount_entry_t *e = mt->entries_list; e != NULL; e = e->next)
        count++;
    return count;
}

static mount_entry_t *mount_table_get_entries_list(mount_table_t *mt) {
    return mt->entries_list;
}

static mount_entry_t *mount_table_create_entry(inode_t *host_dir, inode_t *new_root_dir) {
    mount_entry_t *e = (mount_entry_t *)kmalloc(sizeof(mount_entry_t));

    e->host_dir = *host_dir;
    e->root_dir = *new_root_dir;
    e->sb = new_root_dir->sb;
    e->flags = 0;
    e->ref_count = 0;
    e->next = NULL;

    return e;
}

static void mount_table_destroy_entry(mount_entry_t *e) {
    if (e) kfree(e);
}

static int mount_table_add_entry(mount_table_t *mt, mount_entry_t *e) {
    if (mt->entries_list == 0) {
        mt->entries_list = e;
        e->next = 0;

    } else {
        // add to tail
        mount_entry_t *prev = e;
        while (prev->next != NULL)
            prev = prev->next;
        prev->next = e;
        e->next = 0;

    }

    return OK;
}

static int mount_table_remove_entry(mount_table_t *mt, mount_entry_t *e) {
    if (mt->entries_list == e) {
        mt->entries_list = e->next;
        e->next = NULL;

    } else {
        mount_entry_t *prev = mt->entries_list;
        while (prev != NULL && prev->next != e)
            prev = prev->next;
        if (prev != NULL) {
            prev->next = e->next;
            e->next = NULL;
        }
    }

    return OK;
}

static mount_entry_t *mount_table_find_entry_by_host_dir(mount_table_t *mt, inode_t *n) {
    for (mount_entry_t *e = mt->entries_list; e != NULL; e = e->next) {
        if (inodes.equals(n, &e->host_dir))
            return e;
    }

    return NULL;
}

static mount_entry_t *mount_table_find_entry_by_root_dir(mount_table_t *mt, inode_t *n) {
    for (mount_entry_t *e = mt->entries_list; e != NULL; e = e->next) {
        if (inodes.equals(n, &e->root_dir))
            return e;
    }
    
    return NULL;
}

static void mount_table_destroy(mount_table_t *mt) {
    ASSERT(mt != NULL);
    mount_entry_t *next = mt->entries_list;
    for (mount_entry_t *e = next; e != NULL; e = next) {
        next = e->next;
        mount_table_destroy_entry(e);
    }
    kfree(mt);
}

static struct mount_table_ops mtab_ops = {
    .count_entries          = mount_table_count_entries,
    .get_entries_list       = mount_table_get_entries_list,
    .create_entry           = mount_table_create_entry,
    .destroy_entry          = mount_table_destroy_entry,
    .add_entry              = mount_table_add_entry,
    .remove_entry           = mount_table_remove_entry,
    .find_entry_by_host_dir = mount_table_find_entry_by_host_dir,
    .find_entry_by_root_dir = mount_table_find_entry_by_root_dir,
    .destroy                = mount_table_destroy,
};

mount_table_t *create_mount_table() {
    mount_table_t *mt = (mount_table_t *)kmalloc(sizeof(mount_table_t));
    mt->ops = &mtab_ops;
    mt->entries_list = NULL;
    return mt;
}
