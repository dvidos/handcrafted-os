#ifndef _PCI_H
#define _PCI_H

#include <ctypes.h>

typedef struct pci_device pci_device_t;
typedef struct pci_configuration pci_configuration_t;
typedef struct pci_header_0x00_values pci_header_0x00_values_t;
typedef struct pci_header_0x01_values pci_header_0x01_values_t;
typedef struct pci_header_0x02_values pci_header_0x02_values_t;

struct pci_device_id {
    uint16_t vendor_id;  // vendors unique ids can be discovered online
    uint16_t device_id;  // indicates device per vendor
    uint8_t class_type;  // RO, the device type, tables exist for this
    uint8_t sub_class;   // RO, specific type within class type
};

struct pci_driver {
    char *name;

    // some pointers to functions to init/sleep/read/write etc.
    // they will take a pointer to a pci_device_t struct
    // pci subsystem passes the device id for the driver to see if it will support it
    // the driver should return zero if successfully claiming the device
    int (*probe)(pci_device_t *pci_dev);
};

void register_pci_driver(uint8_t class, uint8_t subclass, struct pci_driver *driver);



void init_pci();
void register_pci_driver(uint8_t class, uint8_t subclass, struct pci_driver *driver);
char *get_pci_class_name(uint8_t class);
char *get_pci_subclass_name(uint8_t class, uint8_t subclass);


// for use by device drivers
uint8_t pci_dev_read_config_byte(pci_device_t *dev, uint8_t offset);
uint16_t pci_dev_read_config_word(pci_device_t *dev, uint8_t offset);
uint32_t pci_dev_read_config_dword(pci_device_t *dev, uint8_t offset);
void pci_dev_write_config_byte(pci_device_t *dev, uint8_t offset, uint8_t value);
void pci_dev_write_config_word(pci_device_t *dev, uint8_t offset, uint16_t value);
void pci_dev_write_config_dword(pci_device_t *dev, uint8_t offset, uint32_t value);



struct pci_header_0x00_values {
    // 0x10
    uint32_t bar0; // base address register
    // 0x14
    uint32_t bar1; // base address register
    // 0x18
    uint32_t bar2; // base address register
    // 0x1C
    uint32_t bar3; // base address register
    // 0x20
    uint32_t bar4; // base address register
    // 0x24
    uint32_t bar5; // base address register
    // 0x28
    uint32_t cardbus_cis_pointer;
    // 0x2C
    uint16_t subsystem_id;
    uint16_t subsystem_vendor_id;
    // 0x30
    uint32_t expansion_rom_base_address;
    // 0x34
    uint32_t reserved0 : 24;
    uint32_t capabilities_pointer : 8;
    // 0x38
    uint32_t reserved1;
    // 0x3C
    uint8_t  max_latency;
    uint8_t  min_grant;
    uint8_t  interrupt_pin;
    uint8_t  interrupt_line;
} __attribute__((packed));

struct pci_header_0x01_values {
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
struct pci_header_0x02_values {
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


// information we collect when enumerating PCI devices
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
        pci_header_0x00_values_t h00;
        pci_header_0x01_values_t h01;
        pci_header_0x02_values_t h02;
    } headers; // union of different header headers (t0, t1, t2 and flat)
} __attribute__((packed));


struct pci_device {
    uint8_t bus_no;
    uint8_t device_no;
    uint8_t func_no;
    pci_configuration_t config;
    void *driver_private_data;
    struct pci_device *next;
} __attribute__((packed));



#endif


