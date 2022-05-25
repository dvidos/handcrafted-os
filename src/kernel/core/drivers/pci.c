#include <stddef.h>
#include "../cpu.h"
#include "../bits.h"
#include "../klog.h"
#include "../memory/kheap.h"

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

struct header_0x00_values {
    uint32_t bar0; // base address register
    uint32_t bar1; // base address register
    uint32_t bar2; // base address register
    uint32_t bar3; // base address register
    uint32_t bar4; // base address register
    uint32_t bar5; // base address register
    uint32_t cardbus_cis_pointer;
    uint16_t subsystem_id;
    uint16_t subsystem_vendor_id;
    uint32_t expansion_rom_base_address;
    uint32_t reserved0 : 24;
    uint32_t capabilities_pointer : 8;
    uint32_t reserved1;
    uint8_t  max_latency;
    uint8_t  min_grant;
    uint8_t  interrupt_pin;
    uint8_t  interrupt_line;
} __attribute__((packed));

struct header_0x01_values {
    uint32_t bar0; // base address register
    uint32_t bar1; // base address register
    uint8_t  secondary_latency_timer;
    uint8_t  subordinate_bus_number;
    uint8_t  secondary_bus_number;
    uint8_t  primary_bus_number;
    uint16_t secondary_status;
    uint8_t  io_limit;
    uint8_t  io_base;
    uint16_t memory_limit;
    uint16_t memory_base;
    uint16_t prefetchable_memory_limit;
    uint16_t prefetchable_memory_base;
    uint32_t prefetchable_base_upper_32_bits;
    uint32_t prefetchable_limit_upper_32_bits;
    uint16_t io_limit_upper_16_bits;
    uint16_t io_base_upper_16_bits;
    uint32_t reseved0 : 24;
    uint32_t capabilities_pointer : 8;
    uint32_t expansion_rom_base_address;
    uint16_t bridge_control;
    uint8_t  interrupt_pin;
    uint8_t  interrupt_line;
} __attribute__((packed));

// note 0x02 header is 56 bytes long, not 48, as the others
struct header_0x02_values {
    uint32_t cardbus_base_address;
    uint16_t secondary_status;
    uint8_t  reserved0;
    uint8_t  offset_of_capabilities_list;
    uint8_t  cardbus_latency_timer;
    uint8_t  subordinate_bus_number;
    uint8_t  cardbus_bus_number;
    uint8_t  pci_bus_number;
    uint32_t memory_base_address_0;
    uint32_t memory_limit_0;
    uint32_t memory_base_address_1;
    uint32_t memory_limit_1;
    uint32_t io_base_address_0;
    uint32_t io_limit_0;
    uint32_t io_base_address_1;
    uint32_t io_limit_1;
    uint16_t bridge_control;
    uint8_t  interrupt_pin;
    uint8_t  interrupt_line;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_device_id;
    uint32_t pc_card_16bit_legacy_mode_base_address;
} __attribute__((packed));

struct pci_configuration {
    uint16_t device_id;  // indicates device per vendor
    uint16_t vendor_id;  // vendors unique ids can be discovered online
    uint16_t status;     // status bits
    uint16_t command;    // command
    uint8_t revision_id; // revision id, provided by the vendor
    uint8_t prog_if;     // type of programming interface, if any
    uint8_t sub_class;   // RO, specific type within class type
    uint8_t class_type;  // RO, the device type, tables exist for this
    uint8_t cache_line_size; // cache size in 32bit units
    uint8_t latency_timer;  // the latency timer in units of pci bus cycles
    uint8_t header_type;
    uint8_t bist;
    union {
        struct header_0x00_values h00;
        struct header_0x01_values h01;
        struct header_0x02_values h02;
    } headers; // union of different header headers (t0, t1, t2 and flat)
} __attribute__((packed));

struct pci_device {
    uint8_t bus_no;
    uint8_t device_no;
    uint8_t func_no;
    struct pci_configuration config;

    struct pci_device *next;
} __attribute__((packed));
typedef struct pci_device pci_device_t;

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

uint32_t pci_address(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    return (
        (uint32_t)0x80000000 |
        (((uint32_t)bus)    << 16) |
        (((uint32_t)device) << 11) |
        (((uint32_t)func)   <<  8) |
        (((uint32_t)offset & (~0x03)))
    );
}

uint8_t pci_read_config_byte(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Read in the data
    uint8_t data = inb(PCI_CONFIG_DATA_PORT + (offset & 0x03));
    return data;
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Read in the data
    uint16_t data = inw(PCI_CONFIG_DATA_PORT + (offset & 0x02));
    return data;
}

uint32_t pci_read_config_dword(uint8_t bus, uint8_t device, uint8_t func, uint8_t offset) {
    // Write out the address
    outl(PCI_CONFIG_ADDRESS_PORT, pci_address(bus, device, func, offset));

    // Read in the data
    uint32_t data = inl(PCI_CONFIG_DATA_PORT);
    return data;
}

pci_device_t *read_pci_device_configuration(uint8_t bus, uint8_t device, uint8_t func) {
    uint32_t reg = pci_read_config_dword(bus, device, func, 0);
    uint16_t vendor_id = (reg & 0xFFFF);
    if (vendor_id == 0xFFFF)
        return NULL;
    
    // so we have a true device, let's read everything
    pci_device_t *dev = kalloc(sizeof(pci_device_t));

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

        // ... 
        

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

void init_pci() {
    klog_debug("Collecting pci devices...\n");
    collect_pci_devices();

    pci_device_t *dev = pci_devices_list;
    while (dev != NULL) {
        klog_debug("B:D:F: %d:%d:%d, vend.id %04x, dev.id %04x, class/sub %02x/%02x, hdr %02x",
            dev->bus_no,
            dev->device_no,
            dev->func_no,
            dev->config.vendor_id,
            dev->config.device_id,
            dev->config.class_type,
            dev->config.sub_class,
            dev->config.header_type
        );
        dev = dev->next;
    }

    /*
        Device 3 on my QEMU environment.
        - Vendor = 8006 (Intel)
        - Device = 100E (Gigabit Ethernet Controller)
        - Class = 02    (Network Controller)
        - Subclass = 00 (Ethernet Controller)

        Device 4 on my QEMU environment.
        - Vendor = 8006 (Intel)
        - Device = 2922 (6 port SATA Controller [AHCI mode])
        - Class = 01    (Mass Storage Controller)
        - Subclass = 06 (Serial ATA Controller)

        B:D:F 0:0:0, vend.id 8086, dev.id 1237, class/sub 06/00, hdr 00
        B:D:F 0:1:0, vend.id 8086, dev.id 7000, class/sub 06/01, hdr 80
        B:D:F 0:1:1, vend.id 8086, dev.id 7010, class/sub 01/01, hdr 00
        B:D:F 0:2:0, vend.id 1234, dev.id 1111, class/sub 03/00, hdr 00
        B:D:F 0:3:0, vend.id 8086, dev.id 100E, class/sub 02/00, hdr 00
        B:D:F 0:4:0, vend.id 8086, dev.id 2922, class/sub 01/06, hdr 00
    */
}


struct pci_driver {
    char *name;
    // some pointers to functions to init/sleep/read/write etc.
    // they will take a pointer to a pci_device_t struct
};

void register_pci_driver(uint8_t class, uint8_t subclass, struct pci_driver *driver) {
    // keep a list of available drivers... etc.
}

