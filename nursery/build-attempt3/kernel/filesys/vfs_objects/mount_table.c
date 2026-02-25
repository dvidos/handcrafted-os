#include "mount_table.h"
#include "../../memory/kheap.h"
#include "../../include/uapi/errors.h"


// THE entry point
static mount_entry_t *mtab_entries_list_head = NULL;


static mount_entry_t *_mtab_get_entries_list() {
    return mtab_entries_list_head;
}

static mount_entry_t *_mtab_create_entry(inode_t *host_dir, inode_t *new_root_dir) {
    mount_entry_t *e = (mount_entry_t *)kmalloc(sizeof(mount_entry_t));

    e->host_dir = host_dir;
    e->root_dir = new_root_dir;
    e->sb = new_root_dir->sb;
    e->flags = 0;
    e->ref_count = 0;
    e->next = NULL;

    return e;
}

static void _mtab_destroy_entry(mount_entry_t *e) {
    if (e) kfree(e);
}

static int _mtab_add_entry(mount_entry_t *e) {
    if (mtab_entries_list_head == 0) {
        mtab_entries_list_head = e;
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

static int _mtab_remove_entry(mount_entry_t *e) {
    if (mtab_entries_list_head == e) {
        mtab_entries_list_head = e->next;
        e->next = NULL;

    } else {
        mount_entry_t *prev = mtab_entries_list_head;
        while (prev != NULL && prev->next != e)
            prev = prev->next;
        if (prev != NULL) {
            prev->next = e->next;
            e->next = NULL;
        }
    }

    return OK;
}

static mount_entry_t *_mtab_find_entry_by_host_dir(inode_t *n) {
    for (mount_entry_t *e = mtab_entries_list_head; e != NULL; e = e->next) {
        if (inodes.equals(n, e->host_dir))
            return e;
    }

    return NULL;
}

static mount_entry_t *_mtab_find_entry_by_root_dir(inode_t *n) {
    for (mount_entry_t *e = mtab_entries_list_head; e != NULL; e = e->next) {
        if (inodes.equals(n, e->root_dir))
            return e;
    }
    
    return NULL;
}

struct mount_table_ops mtab = {
    .get_entries_list       = _mtab_get_entries_list,
    .create_entry           = _mtab_create_entry,
    .destroy_entry          = _mtab_destroy_entry,
    .add_entry              = _mtab_add_entry,
    .remove_entry           = _mtab_remove_entry,
    .find_entry_by_host_dir = _mtab_find_entry_by_host_dir,
    .find_entry_by_root_dir = _mtab_find_entry_by_root_dir,
};
