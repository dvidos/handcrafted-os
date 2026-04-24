#pragma once
#include "../pci/pci_device.h"


void discover_and_register_all_sata_block_devices(list_t *pci_devices_list);

