#include "../../include/bits.h"
#include "../arch/cpu.h"
#include "../logger/logger.h"
#include "../memory/kheap.h"
#include "../klib/string.h"
#include "pci_device.h"
#include "../devices.h"

MODULE("PCI_DEV", LOG_LEVEL_WARN);

// i think I need to do PCI discovery, to find any hard disks (even SATA) and USB hubs / sticks.
// currently following https://wiki.osdev.org/PCI

// all pci devices offer 256 registers for configuration
// i think this is 64 registers of 32bit.
// there are 256 busses, each can have about 10 devices

// vendor ids are here: https://pcisig.com/membership/member-companies
// an example of how to write a device driver for linux is here
// https://olegkutkov.me/2021/01/07/writing-a-pci-device-driver-for-linux/



// 32-bit ports
#define PCI_CONFIG_ADDRESS_PORT   0xCF8
#define PCI_CONFIG_DATA_PORT      0xCFC


static inline uint32_t pci_address(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    // the format of the config dword is the following:
    // bit     31 (highest) - should access to the PCI_CONFIG_DATA_PORT register be transleated to config cycle
    // bits 30-24 (reserved)
    // bits 23-16 (8 bits) - bus number
    // bits 15-11 (5 bits) - device number
    // bits  10-8 (3 bits) - device function
    // bits   7-0 (8 bits) - register offset (6 bits register, two least signif bits always zero)
    return (
        (uint32_t)0x80000000 |
        (((uint32_t)bus)    << 16) |
        (((uint32_t)device) << 11) |
        (((uint32_t)func)   <<  8) |
        (((uint32_t)offset & (~0x03)))
    );
}

static inline uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));
    return inb(PCI_CONFIG_DATA_PORT + (offset & 0x03));
}

static inline uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));
    return inw(PCI_CONFIG_DATA_PORT + (offset & 0x02));
}

static inline uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));
    return inl(PCI_CONFIG_DATA_PORT);
}

static inline void pci_write_config_byte(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint8_t value) {
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));
    outw(PCI_CONFIG_DATA_PORT + (offset & 0x03), value);
}

static inline void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint16_t value) {
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));
    outw(PCI_CONFIG_DATA_PORT + (offset & 0x02), value);
}

static inline void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));
    outl(PCI_CONFIG_DATA_PORT, value);
}

// ------------------------------------------------------------------------------------------------

static const char *pci_device_get_class_name(pci_device_t *dev) {
    switch (dev->config.class_type) {
        case 0x00: return "Unclassified";
        case 0x01: return "Mass Storage Controller";
        case 0x02: return "Network Controller";
        case 0x03: return "Display Controller";
        case 0x04: return "Multimedia Controller";
        case 0x05: return "Memory Controller";
        case 0x06: return "Bridge";
        case 0x07: return "Simple Communication Controller";
        case 0x08: return "Base System Peripheral";
        case 0x09: return "Input Device Controller";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus Controller";
        case 0x0D: return "Wireless Controller";
        case 0x0E: return "Intelligent Controller";
        case 0x0F: return "Satellite Communication Controller";
        case 0x10: return "Encryption Controller";
        case 0x11: return "Signal Processing Controller";
        case 0x12: return "Processing Accelerator";
        case 0x13: return "Non-Essential Instrumentation";
        case 0x40: return "Co-Processor";
    }

    return "Unknown";
}

static const char *pci_device_get_subclass_name(pci_device_t *dev) {
    switch (dev->config.class_type) {
        case 0x00: // Unclassified
            switch (dev->config.sub_class) {
                case 0x0: return "Non-VGA-Compatible Unclassified Device";
                case 0x1: return "VGA-Compatible Unclassified Device";
            }
            break;
        case 0x01: // Mass Storage Controller
            switch (dev->config.sub_class) {
                case 0x0: return "SCSI Bus Controller";
                case 0x1: return "IDE Controller";
                case 0x2: return "Floppy Disk Controller";
                case 0x3: return "IPI Bus Controller";
                case 0x4: return "RAID Controller";
                case 0x5: return "ATA Controller";
                case 0x6: return "Serial ATA Controller";
                case 0x7: return "Serial Attached SCSI Controller";
                case 0x8: return "Non-Volatile Memory Controller";
            }
            break;
        case 0x02: // Network Controller
            switch (dev->config.sub_class) {
                case 0x0: return "Ethernet Controller";
                case 0x1: return "Token Ring Controller";
                case 0x2: return "FDDI Controller";
                case 0x3: return "ATM Controller";
                case 0x4: return "ISDN Controller";
                case 0x5: return "WorldFip Controller";
                case 0x6: return "PICMG 2.14 Multi Computing Controller";
                case 0x7: return "Infiniband Controller";
                case 0x8: return "Fabric Controller";
            }
            break;
        case 0x03: // Display Controller
            switch (dev->config.sub_class) {
                case 0x0: return "VGA Compatible Controller";
                case 0x1: return "XGA Controller";
                case 0x2: return "3D Controller (Not VGA-Compatible)";
            }
            break;
        case 0x04: // Multimedia Controller
            switch (dev->config.sub_class) {
                case 0x0: return "Multimedia Video Controller";
                case 0x1: return "Multimedia Audio Controller";
                case 0x2: return "Computer Telephony Device";
                case 0x3: return "Audio Device";
            }
            break;
        case 0x05: // Memory Controller
            switch (dev->config.sub_class) {
                case 0x0: return "RAM Controller";
                case 0x1: return "Flash Controller";
            }
            break;
        case 0x06: // Bridge
            switch (dev->config.sub_class) {
                case 0x0: return "Host Bridge";
                case 0x1: return "ISA Bridge";
                case 0x2: return "EISA Bridge";
                case 0x3: return "MCA Bridge";
                case 0x4: return "PCI-to-PCI Bridge";
                case 0x5: return "PCMCIA Bridge";
                case 0x6: return "NuBus Bridge";
                case 0x7: return "CardBus Bridge";
                case 0x8: return "RACEway Bridge";
                case 0x9: return "PCI-to-PCI Bridge";
                case 0xA: return "InfiniBand-to-PCI Host Bridge";
            }
            break;
        case 0x07: // Simple Communication Controller
            switch (dev->config.sub_class) {
                case 0x0: return "Serial Controller";
                case 0x1: return "Parallel Controller";
                case 0x2: return "Multiport Serial Controller";
                case 0x3: return "Modem";
                case 0x4: return "IEEE 488.1/2 (GPIB) Controller";
                case 0x5: return "Smart Card Controller";
            }
            break;
        case 0x08: // Base System Peripheral
            switch (dev->config.sub_class) {
                case 0x0: return "PIC";
                case 0x1: return "DMA Controller";
                case 0x2: return "Timer";
                case 0x3: return "RTC Controller";
                case 0x4: return "PCI Hot-Plug Controller";
                case 0x5: return "SD Host controller";
                case 0x6: return "IOMMU";
            }
            break;
        case 0x09: // Input Device Controller
            switch (dev->config.sub_class) {
                case 0x0: return "Keyboard Controller";
                case 0x1: return "Digitizer Pen";
                case 0x2: return "Mouse Controller";
                case 0x3: return "Scanner Controller";
                case 0x4: return "Gameport Controller";
            }
            break;
        case 0x0A: // Docking Station
            switch (dev->config.sub_class) {
                case 0x0: return "Generic";
            }
            break;
        case 0x0B: // Processor
            switch (dev->config.sub_class) {
                case 0x0: return "386";
                case 0x1: return "486";
                case 0x2: return "Pentium";
                case 0x3: return "Pentium Pro";
                case 0x10: return "Alpha";
                case 0x20: return "PowerPC";
                case 0x30: return "MIPS";
                case 0x40: return "Co-Processor";
            }
            break;
        case 0x0C: // Serial Bus Controller
            switch (dev->config.sub_class) {
                case 0x0: return "FireWire (IEEE 1394) Controller";
                case 0x1: return "ACCESS Bus Controller";
                case 0x2: return "SSA";
                case 0x3: return "USB Controller";
                case 0x4: return "Fibre Channel";
                case 0x5: return "SMBus Controller";
                case 0x6: return "InfiniBand Controller";
                case 0x7: return "IPMI Interface";
                case 0x8: return "SERCOS Interface (IEC 61491)";
                case 0x9: return "CANbus Controller";
            }
            break;
        case 0x0D: // Wireless Controller
            switch (dev->config.sub_class) {
                case 0x0: return "iRDA Compatible Controller";
                case 0x1: return "Consumer IR Controller";
                case 0x10: return "RF Controller";
                case 0x11: return "Bluetooth Controller";
                case 0x12: return "Broadband Controller";
                case 0x20: return "Ethernet Controller (802.1a)";
                case 0x21: return "Ethernet Controller (802.1b)";
            }
            break;
    }

    return "Unknown";
}

static uint8_t pci_device_read_config_byte(pci_device_t *dev, uint8_t offset) {
    return pci_read_config_byte(dev->bus_no, dev->device_no, dev->func_no, offset);
}

static uint16_t pci_device_read_config_word(pci_device_t *dev, uint8_t offset) {
    return pci_read_config_word(dev->bus_no, dev->device_no, dev->func_no, offset);
}

static uint32_t pci_device_read_config_dword(pci_device_t *dev, uint8_t offset) {
    return pci_read_config_dword(dev->bus_no, dev->device_no, dev->func_no, offset);
}

static void pci_device_write_config_byte(pci_device_t *dev, uint8_t offset, uint8_t value) {
    pci_write_config_byte(dev->bus_no, dev->device_no, dev->func_no, offset, value);
}

static void pci_device_write_config_word(pci_device_t *dev, uint8_t offset, uint16_t value) {
    pci_write_config_word(dev->bus_no, dev->device_no, dev->func_no, offset, value);
}

static void pci_device_write_config_dword(pci_device_t *dev, uint8_t offset, uint32_t value) {
    pci_write_config_dword(dev->bus_no, dev->device_no, dev->func_no, offset, value);
}

static struct pci_device_ops ops = {
    .class_name         = pci_device_get_class_name,
    .subclass_name      = pci_device_get_subclass_name,
    .read_config_byte   = pci_device_read_config_byte,
    .read_config_word   = pci_device_read_config_word,
    .read_config_dword  = pci_device_read_config_dword,
    .write_config_byte  = pci_device_write_config_byte,
    .write_config_word  = pci_device_write_config_word,
    .write_config_dword = pci_device_write_config_dword,
};

pci_device_t *create_pci_device(
    uint8_t bus,
    uint8_t device,
    uint8_t func,
    uint8_t class,
    uint8_t subclass
) {
    pci_device_t *dev = (pci_device_t *)kmalloc(sizeof(pci_device_t *));
    memset(dev, 0, sizeof(pci_device_t));

    dev->bus_no = bus;
    dev->device_no = device;
    dev->func_no = func;
    dev->config.class_type = class;
    dev->config.sub_class = subclass;


    // and the rest?
    
    dev->ops = &ops;

    return dev;
}

// --------------------------------------------------------------------------------------

pci_device_t *discover_pci_device(uint8_t bus, uint8_t device, uint8_t func) {
    uint32_t reg = pci_read_config_dword(bus, device, func, 0);
    uint16_t vendor_id = (reg & 0xFFFF);
    if (vendor_id == 0xFFFF)
        return NULL;
    
    // so we have a true device, let's read everything
    pci_device_t *dev = kmalloc(sizeof(pci_device_t));
    memset(dev, 0, sizeof(pci_device_t));

    dev->bus_no = bus;
    dev->device_no = device;
    dev->func_no = func;

    dev->config.vendor_id = vendor_id;
    dev->config.device_id = reg >> 16;

    reg = pci_read_config_dword(bus, device, func, 0x4);
    dev->config.status =  reg >> 16;
    dev->config.command = reg & 0xFFFF;

    reg = pci_read_config_dword(bus, device, func, 0x8);
    dev->config.class_type  = (reg >> 24) & 0xFF;
    dev->config.sub_class   = (reg >> 16) & 0xFF;
    dev->config.prog_if     = (reg >>  8) & 0xFF;
    dev->config.revision_id = (reg & 0xFF);

    reg = pci_read_config_dword(bus, device, func, 0xC);
    dev->config.bist            = (reg >> 24) & 0xFF;
    dev->config.header_type     = (reg >> 16) & 0xFF;
    dev->config.latency_timer   = (reg >>  8) & 0xFF;
    dev->config.cache_line_size = (reg & 0xFF);

    // friendly strings:
    char id_buffer[16];   // "pci:00:1f.2"
    char name_buffer[128]; // "PCI device 00:1f.2 (class 01/01, Intel IDE Controller)"
    sprintfn(id_buffer, sizeof(id_buffer), "pci:%02x:%02x.%x", dev->bus_no, dev->device_no, dev->func_no);
    sprintfn(name_buffer, sizeof(name_buffer), "PCI device %02x:%02x.%x (%02x/%02x, %s %s)", 
        dev->bus_no, dev->device_no, dev->func_no,
        dev->config.class_type, dev->config.sub_class,
        pci_device_get_class_name(dev),
        pci_device_get_subclass_name(dev)
    );
    dev->id = strdup(id_buffer);
    dev->name = strdup(name_buffer);


    uint8_t head_type = dev->config.header_type & 0x7F;
    if (head_type == 0x00) {

        dev->config.headers.h00.bar0 = pci_read_config_dword(bus, device, func, 0x10);
        dev->config.headers.h00.bar1 = pci_read_config_dword(bus, device, func, 0x14);
        dev->config.headers.h00.bar2 = pci_read_config_dword(bus, device, func, 0x18);
        dev->config.headers.h00.bar3 = pci_read_config_dword(bus, device, func, 0x1C);
        dev->config.headers.h00.bar4 = pci_read_config_dword(bus, device, func, 0x20);
        dev->config.headers.h00.bar5 = pci_read_config_dword(bus, device, func, 0x24);
        dev->config.headers.h00.cardbus_cis_pointer = pci_read_config_dword(bus, device, func, 0x28);

        reg = pci_read_config_dword(bus, device, func, 0x2C);
        dev->config.headers.h00.subsystem_id = (reg >> 16);
        dev->config.headers.h00.subsystem_vendor_id = (reg & 0xFFFF);

        reg = pci_read_config_dword(bus, device, func, 0x2C);
        dev->config.headers.h00.expansion_rom_base_address = pci_read_config_dword(bus, device, func, 0x30);

        reg = pci_read_config_dword(bus, device, func, 0x34);
        dev->config.headers.h00.capabilities_pointer = (reg & 0xFF);

        reg = pci_read_config_dword(bus, device, func, 0x38);
        dev->config.headers.h00.max_latency = (reg >> 24);
        dev->config.headers.h00.min_grant = (reg >> 16);
        dev->config.headers.h00.interrupt_pin = (reg >>  8);
        dev->config.headers.h00.interrupt_line = reg & 0xFF;

    } else if (head_type == 0x01) {

        dev->config.headers.h01.bar0 = pci_read_config_dword(bus, device, func, 0x10);
        dev->config.headers.h01.bar1 = pci_read_config_dword(bus, device, func, 0x14);

        reg = pci_read_config_dword(bus, device, func, 0x18);
        dev->config.headers.h01.secondary_latency_timer = reg >> 24;
        dev->config.headers.h01.subordinate_bus_number = reg >> 16;
        dev->config.headers.h01.secondary_bus_number = reg >> 8;
        dev->config.headers.h01.primary_bus_number = reg & 0xFF;

        reg = pci_read_config_dword(bus, device, func, 0x1C);
        dev->config.headers.h01.secondary_status = reg >> 16;
        dev->config.headers.h01.io_limit = reg >> 8;
        dev->config.headers.h01.io_base = reg & 0xFF;

        reg = pci_read_config_dword(bus, device, func, 0x20);
        dev->config.headers.h01.memory_limit = reg >> 16;
        dev->config.headers.h01.memory_base = reg & 0xFFFF;

        reg = pci_read_config_dword(bus, device, func, 0x24);
        dev->config.headers.h01.prefetchable_memory_limit = reg >> 16;
        dev->config.headers.h01.prefetchable_memory_base = reg & 0xFFFF;

        dev->config.headers.h01.prefetchable_base_upper_32_bits = pci_read_config_dword(bus, device, func, 0x28);
        dev->config.headers.h01.prefetchable_limit_upper_32_bits = pci_read_config_dword(bus, device, func, 0x2C);

        reg = pci_read_config_dword(bus, device, func, 0x30);
        dev->config.headers.h01.io_limit_upper_16_bits = reg >> 16;
        dev->config.headers.h01.io_base_upper_16_bits = reg & 0xFFFF;

        reg = pci_read_config_dword(bus, device, func, 0x34);
        dev->config.headers.h01.capabilities_pointer = reg & 0xFF;

        dev->config.headers.h01.expansion_rom_base_address = pci_read_config_dword(bus, device, func, 0x38);

        reg = pci_read_config_dword(bus, device, func, 0x3C);
        dev->config.headers.h01.bridge_control = reg >> 16;
        dev->config.headers.h01.interrupt_pin = reg >> 8;
        dev->config.headers.h01.interrupt_line = reg & 0xFF;

    } else if (head_type == 0x02) {
        // fill this in... 
    }

    return dev;
}

void discover_all_pci_devices() {
    pci_device_t *dev;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            dev = discover_pci_device((uint8_t)bus, device, 0);
            if (!dev) continue;
            register_pci_device(dev);

            if (!IS_BIT(dev->config.header_type, 7))
            continue;
            
            // if device supports many functions, we should get information for them as well
            // we already have read func 0
            for (uint8_t func = 1; func < 8; func++) {
                dev = discover_pci_device(bus, device, func);
                if (!dev) continue;
                register_pci_device(dev);
            }
        }
    }
}

