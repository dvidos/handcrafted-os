#include "devices.h"
#include "../klib/string.h"
#include "../klib/list.h"
#include "../utils/logger.h"

MODULE("DEV_REG", LOG_LEVEL_WARN);

list_t pci_devices_list = { .first = 0, .last = 0, .count = 0, .node_struct_offset = offsetof(pci_device_t, list_node) };
list_t block_devices_list = { .first = 0, .last = 0, .count = 0, .node_struct_offset = offsetof(block_device_t, list_node) };
list_t char_devices_list = { .first = 0, .last = 0, .count = 0, .node_struct_offset = offsetof(char_device_t, list_node) };


void register_pci_device(pci_device_t *dev) {
    list_append(&pci_devices_list, dev);
}

void unregister_pci_device(pci_device_t *dev) {
    list_remove(&pci_devices_list, dev);
}

static bool pci_device_id_is(void *item, void *context) { 
    return strcmp(((pci_device_t *)item)->id, (char *)context) == 0;
}

pci_device_t *get_pci_device_by_id(const char *id) {
    return list_find_first(&pci_devices_list, pci_device_id_is, (void *)id);
}

void log_all_pci_devices() {
    log_info("PCI Devices:");
    list_foreach(&pci_devices_list, pci_device_t, dev) {
        log_info("  - %-12s  %s", dev->id, dev->name);
    }
}


// ----------------------------------------------------------------

void register_block_device(block_device_t *dev) {
    list_append(&block_devices_list, dev);
}

void unregister_block_device(block_device_t *dev) {
    list_remove(&block_devices_list, dev);
}

static bool block_device_id_is(void *item, void *context) { 
    return strcmp(((block_device_t *)item)->id, (char *)context) == 0;
}

block_device_t *get_block_device_by_id(const char *id) {
    return list_find_first(&block_devices_list, block_device_id_is, (void *)id);
}

void log_all_block_devices() {
    log_info("Block Devices:");
    list_foreach(&block_devices_list, block_device_t, dev) {
        log_info("  - %-8s  %s", dev->id, dev->name);
    }
}

// ----------------------------------------------------------------

void register_char_device(char_device_t *dev) {
    list_append(&char_devices_list, dev);
}

void unregister_char_device(char_device_t *dev) {
    list_remove(&char_devices_list, dev);
}

static bool char_device_id_is(void *item, void *context) { 
    return strcmp(((char_device_t *)item)->id, (char *)context) == 0;
}

char_device_t *get_char_device_by_id(const char *id) {
    return list_find_first(&char_devices_list, char_device_id_is, (void *)id);
}

void log_all_char_devices() {
    log_info("Char Devices:");
    list_foreach(&char_devices_list, char_device_t, dev) {
        log_info("  - %-8s  %s", dev->id, dev->name);
    }
}


