#pragma once
#include "pci/pci_device.h"
#include "block/block_device.h"


extern list_t pci_devices_list;
extern list_t block_devices_list;


void register_pci_device(pci_device_t *dev);
void unregister_pci_device(pci_device_t *dev);
pci_device_t *get_pci_device_by_id(const char *id);
void log_all_pci_devices();


void register_block_device(block_device_t *dev);
void unregister_block_device(block_device_t *dev);
block_device_t *get_block_device_by_id(const char *id);
void log_all_block_devices();
