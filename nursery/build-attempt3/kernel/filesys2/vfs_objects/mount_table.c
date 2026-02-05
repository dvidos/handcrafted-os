#include "mount_table.h"
#include "../../memory/kheap.h"


// THE entry point
mount_entry_t *mtab_entries_list_head = NULL;


mount_entry_t *create_mount_entry(file_descriptor_t *host_dir, file_descriptor_t *new_root_dir) {
    mount_entry_t *e = (mount_entry_t *)kmalloc(sizeof(mount_entry_t));

    e->host_dir = host_dir;
    e->root_dir = new_root_dir;
    e->sb = new_root_dir->sb;
    e->flags = 0;
    e->ref_count = 0;
    e->next = NULL;

    return e;
}

void destroy_mount_entry(mount_entry_t *e) {
    if (e) kfree(e);
}

void mtab_mount(mount_entry_t *e) {
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
}

void mtab_unmount(mount_entry_t *e) {
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
}

mount_entry_t *mtab_find_by_host_dir(file_descriptor_t *fd) {
    for (mount_entry_t *e = mtab_entries_list_head; e != NULL; e = e->next) {
        if (file_descriptors.equals(fd, e->host_dir))
            return e;
    }

    return NULL;
}

mount_entry_t *mtab_find_by_root_dir(file_descriptor_t *fd) {
    for (mount_entry_t *e = mtab_entries_list_head; e != NULL; e = e->next) {
        if (file_descriptors.equals(fd, e->root_dir))
            return e;
    }
    
    return NULL;
}
