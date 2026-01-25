#pragma once

#include "../dependencies/sector_device.h"

sector_device *new_partition_sector_device(sector_device *parent, uint32_t start_sector, uint32_t sector_count);
