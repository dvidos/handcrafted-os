#include <stddef.h>
#include "../cpu.h"
#include "../bits.h"
#include "../klog.h"
#include "../memory/kheap.h"
#include "pci.h"

// i think I need to do PCI discovery
// to find any hard disks (even SATA)
// and USB hubs / sticks.
// currently following https://wiki.osdev.org/PCI

// all pci devices offer 256 registers for configuration
// i think this is 64 registers of 32bit.
// there are 256 busses, each can have about 10 devices

// vendor ids are here: https://pcisig.com/membership/member-companies
// an example of how to write a device driver for linux is here
// https://olegkutkov.me/2021/01/07/writing-a-pci-device-driver-for-linux/



pci_device_t *pci_devices_list = NULL;


// 32-bit ports
#define PCI_CONFIG_ADDRESS_PORT   0xCF8
#define PCI_CONFIG_DATA_PORT      0xCFC

// the format of the config dword is the following:
// bit 31 (highest) - should access to the PCI_CONFIG_DATA_PORT register be transleated to config cycle
// bits 30-24 (reserved)
// bits 23-16 (8 bits) - bus number
// bits 15-11 (5 bits) - device number
// bits 10-8 (3 bits) - device function
// bits 7-0 (8 bits) - register offset (6 bits register, two least signif bits always zero)

static uint32_t pci_address(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    return (
        (uint32_t)0x80000000 |
        (((uint32_t)bus)    << 16) |
        (((uint32_t)device) << 11) |
        (((uint32_t)func)   <<  8) |
        (((uint32_t)offset & (~0x03)))
    );
}

static uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Read in the data
    uint8_t data = inb(PCI_CONFIG_DATA_PORT + (offset & 0x03));
    return data;
}

static uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Read in the data
    uint16_t data = inw(PCI_CONFIG_DATA_PORT + (offset & 0x02));
    return data;
}

static uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Read in the data
    uint32_t data = inl(PCI_CONFIG_DATA_PORT);
    return data;
}

static void pci_write_config_byte(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint8_t value) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Write the data
    outw(PCI_CONFIG_DATA_PORT + (offset & 0x03), value);
}

static void pci_write_config_word(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint16_t value) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Write the data
    outw(PCI_CONFIG_DATA_PORT + (offset & 0x02), value);
}

static void pci_write_config_dword(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset, uint32_t value) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Write the data
    outl(PCI_CONFIG_DATA_PORT, value);
}

uint8_t pci_dev_read_config_byte(pci_device_t *dev, uint8_t offset) {
    return pci_read_config_byte(dev->bus_no, dev->device_no, dev->func_no, offset);
}

uint16_t pci_dev_read_config_word(pci_device_t *dev, uint8_t offset) {
    return pci_read_config_word(dev->bus_no, dev->device_no, dev->func_no, offset);
}

uint32_t pci_dev_read_config_dword(pci_device_t *dev, uint8_t offset) {
    return pci_read_config_dword(dev->bus_no, dev->device_no, dev->func_no, offset);
}

void pci_dev_write_config_byte(pci_device_t *dev, uint8_t offset, uint8_t value) {
    pci_write_config_byte(dev->bus_no, dev->device_no, dev->func_no, offset, value);
}

void pci_dev_write_config_word(pci_device_t *dev, uint8_t offset, uint16_t value) {
    pci_write_config_word(dev->bus_no, dev->device_no, dev->func_no, offset, value);
}

void pci_dev_write_config_dword(pci_device_t *dev, uint8_t offset, uint32_t value) {
    pci_write_config_dword(dev->bus_no, dev->device_no, dev->func_no, offset, value);
}


pci_device_t *read_pci_device_configuration(uint8_t bus, uint8_t device, uint8_t func) {
    uint32_t reg = pci_read_config_dword(bus, device, func, 0);
    uint16_t vendor_id = (reg & 0xFFFF);
    if (vendor_id == 0xFFFF)
        return NULL;
    
    // so we have a true device, let's read everything
    pci_device_t *dev = kmalloc(sizeof(pci_device_t));

    dev->bus_no = bus;
    dev->device_no = device;
    dev->func_no = func;
    dev->next = NULL;

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

void collect_pci_devices() {
    // should free existing ones, before allocating new ones
    pci_devices_list = NULL;

    pci_device_t *dev;
    pci_device_t *tail = NULL;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            dev = read_pci_device_configuration((uint8_t)bus, device, 0);
            if (dev == NULL)
                continue;
            
            if (pci_devices_list == NULL)
                pci_devices_list = dev;
            else
                tail->next = dev;
            tail = dev;

            // if device supports many functions, we should get information for them as well
            if (IS_BIT(dev->config.header_type, 7)) {
                // we already have read func 0
                for (uint8_t func = 1; func < 8; func++) {
                    dev = read_pci_device_configuration(bus, device, func);
                    if (dev == NULL)
                        break;
                    
                    tail->next = dev;
                    tail = dev;
                }
            }
        }
    }
}

void test_pci_device(uint8_t dev_no) {
    klog_debug("Testing device %d");
    uint8_t bytes[16];
    uint16_t words[8];
    uint32_t dwords[4];

    if (dev_no == 3) {
        // klog_debug("Device 3 on my QEMU environment.");
        klog_debug("- Vendor = 8006 (Intel)");
        klog_debug("- Device = 100E (Gigabit Ethernet Controller)");
        klog_debug("- Class = 02    (Network Controller)");
        klog_debug("- Subclass = 00 (Ethernet Controller)");
    } else if (dev_no == 4) {
        // klog_debug("Device 4 on my QEMU environment.");
        klog_debug("- Vendor = 8006 (Intel)");
        klog_debug("- Device = 2922 (6 port SATA Controller [AHCI mode])");
        klog_debug("- Class = 01    (Mass Storage Controller)");
        klog_debug("- Subclass = 06 (Serial ATA Controller)");
    }
    for (int i = 0; i < 16; i++)
        bytes[i] = pci_read_config_byte(0, dev_no, 0, i);
    
    for (int i = 0; i < 8; i++)
        words[i] = pci_read_config_word(0, dev_no, 0, i << 1);
    
    for (int i = 0; i < 4; i++)
        dwords[i] = pci_read_config_dword(0, dev_no, 0, i << 2);
    
    klog_debug("Using  8 bits: %02x %02x %02x %02x", bytes[0], bytes[1], bytes[2], bytes[3]);
    klog_debug("               %02x %02x %02x %02x", bytes[4], bytes[5], bytes[6], bytes[7]);
    klog_debug("               %02x %02x %02x %02x", bytes[8], bytes[9], bytes[10], bytes[11]);
    klog_debug("               %02x %02x %02x %02x", bytes[12], bytes[13], bytes[14], bytes[15]);
    klog_debug("Using 16 bits: %04x %04x", words[0], words[1]);
    klog_debug("               %04x %04x", words[2], words[3]);
    klog_debug("               %04x %04x", words[4], words[5]);
    klog_debug("               %04x %04x", words[6], words[7]);
    klog_debug("Using 32 bits: %08x", dwords[0]);
    klog_debug("               %08x", dwords[1]);
    klog_debug("               %08x", dwords[2]);
    klog_debug("               %08x", dwords[3]);

    pci_device_t *pci_dev = read_pci_device_configuration(0, dev_no, 0);
    klog_debug("Using configuration header");
    klog_debug("  vendor %04x, dev.id %04x, status %04x, class %02x, subclass %02x, hdr %02x",
        pci_dev->config.vendor_id,
        pci_dev->config.device_id,
        pci_dev->config.status,
        pci_dev->config.class_type,
        pci_dev->config.sub_class,
        pci_dev->config.header_type
    );

    union {
        struct {
            uint8_t high_bit : 1; // <-- C assigns this to the LS bit
            uint8_t some_bits: 6;
            uint8_t low_bit: 1;   // <-- C assigns this to the MS bit
        } __attribute__((packed)) bits;
        uint8_t value;
    } __attribute__((packed)) u;

    u.value = 0x80;
    klog_debug("Bits distribution: value=%x, high bit=%x, low bit=%x", u.value, u.bits.high_bit, u.bits.low_bit);
    u.value = 0x01;
    klog_debug("Bits distribution: value=%x, high bit=%x, low bit=%x", u.value, u.bits.high_bit, u.bits.low_bit);
}

char *get_pci_class_name(uint8_t class) {
    switch (class) {
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

char *get_pci_subclass_name(uint8_t class, uint8_t subclass) {
    switch (class) {
        case 0x00: // Unclassified
            switch (subclass) {
                case 0x0: return "Non-VGA-Compatible Unclassified Device";
                case 0x1: return "VGA-Compatible Unclassified Device";
            }
            break;
        case 0x01: // Mass Storage Controller
            switch (subclass) {
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
            switch (subclass) {
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
            switch (subclass) {
                case 0x0: return "VGA Compatible Controller";
                case 0x1: return "XGA Controller";
                case 0x2: return "3D Controller (Not VGA-Compatible)";
            }
            break;
        case 0x04: // Multimedia Controller
            switch (subclass) {
                case 0x0: return "Multimedia Video Controller";
                case 0x1: return "Multimedia Audio Controller";
                case 0x2: return "Computer Telephony Device";
                case 0x3: return "Audio Device";
            }
            break;
        case 0x05: // Memory Controller
            switch (subclass) {
                case 0x0: return "RAM Controller";
                case 0x1: return "Flash Controller";
            }
            break;
        case 0x06: // Bridge
            switch (subclass) {
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
            switch (subclass) {
                case 0x0: return "Serial Controller";
                case 0x1: return "Parallel Controller";
                case 0x2: return "Multiport Serial Controller";
                case 0x3: return "Modem";
                case 0x4: return "IEEE 488.1/2 (GPIB) Controller";
                case 0x5: return "Smart Card Controller";
            }
            break;
        case 0x08: // Base System Peripheral
            switch (subclass) {
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
            switch (subclass) {
                case 0x0: return "Keyboard Controller";
                case 0x1: return "Digitizer Pen";
                case 0x2: return "Mouse Controller";
                case 0x3: return "Scanner Controller";
                case 0x4: return "Gameport Controller";
            }
            break;
        case 0x0A: // Docking Station
            switch (subclass) {
                case 0x0: return "Generic";
            }
            break;
        case 0x0B: // Processor
            switch (subclass) {
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
            switch (subclass) {
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
            switch (subclass) {
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

void init_pci() {
    klog_debug("Collecting PCI devices...\n");
    collect_pci_devices();

    pci_device_t *dev = pci_devices_list;
    while (dev != NULL) {
        klog_info("B:D:F: %d:%d:%d, vend.id %04x, dev.id %04x, class/sub %02x/%02x, hdr %02x",
            dev->bus_no,
            dev->device_no,
            dev->func_no,
            dev->config.vendor_id,
            dev->config.device_id,
            dev->config.class_type,
            dev->config.sub_class,
            dev->config.header_type
        );
        klog_info("              %s, %s", 
            get_pci_class_name(dev->config.class_type),
            get_pci_subclass_name(dev->config.class_type, dev->config.sub_class)
        );
        if ((dev->config.header_type & 0x3) == 0) {
            klog_info("              BARs: %x %x %x %x %x %x", 
                dev->config.headers.h00.bar0,
                dev->config.headers.h00.bar1,
                dev->config.headers.h00.bar2,
                dev->config.headers.h00.bar3,
                dev->config.headers.h00.bar4,
                dev->config.headers.h00.bar5
            );
        }
        dev = dev->next;
    }
}


void register_pci_driver(uint8_t class, uint8_t subclass, struct pci_driver *driver) {
    // keep a list of available drivers
}

