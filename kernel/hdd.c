#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "ports.h"
#include "string.h"
#include "screen.h"
#include "timer.h"
#include "klog.h"


// driver for ATA drives, PIO (vs DMA) access for now.
// based mainly on 
// http://www.osdever.net/tutorials/view/lba-hdd-access-via-pio
// and https://wiki.osdev.org/ATA_PIO_Mode

#define FIRST_BUS_PORT     0x1F0 // through 0x1F7
#define SECOND_BUS_PORT    0x170 // through 0x177
#define THIRD_BUS_PORT     0x1E8 // through 1EF
#define FOURTH_BUS_PORT    0x168 // through 16F

#define FIRST_CONTROL_REG_PORT   0x3F6
#define SECOND_CONTROL_REG_PORT  0x376
#define THIRD_CONTROL_REG_PORT   0x3E6
#define FOURTH_CONTROL_REG_PORT  0x366

// IRQ for primary controller is 14, for secondary it is 15


struct controller_info {
    uint16_t io_port_base;
    uint16_t control_port_base;
    bool exists;
};
typedef struct controller_info controller_info_t;

struct drive_info {
    uint8_t controller_no; // index to the controllers array
    uint8_t ctrl_drive_no; // drive_no for this controller 0/1
    bool exists;
};
typedef struct drive_info drive_info_t;

// read/write in LBA (Linear Block Address) mode, each block is 512 bytes long.
// most drives support LBA28 (28 bits for block number)
// not all support LBA48 (48 bits for block number)
uint8_t sector_buffer[512];
controller_info_t controllers[4];
drive_info_t drives[4];

bool is_controller_present(int controller_no);
bool is_drive_present(int drive);
static void prepare_lba28_operation(int drive, uint64_t block_num, uint8_t command);
static void prepare_lba48_operation(int drive, uint64_t block_num, uint8_t command);
static void wait_for_controller_to_be_ready(int drive);
static void transfer_disk_to_memory(int drive);
static void transfer_memory_to_disk(int drive);
void read_block_lba28(uint8_t drive, uint64_t block_num);
void write_block_lba28(uint8_t drive, uint64_t block_num);
void read_block_lba48(uint8_t drive, uint64_t block_num);
void write_block_lba48(uint8_t drive, uint64_t block_num);

void test_hdd() {
    controllers[0].io_port_base = 0x1F0;
    controllers[1].io_port_base = 0x170;
    controllers[2].io_port_base = 0x1E8;
    controllers[3].io_port_base = 0x168;

    controllers[0].control_port_base = 0x3F6;
    controllers[1].control_port_base = 0x376;
    controllers[2].control_port_base = 0x3E6;
    controllers[3].control_port_base = 0x366;

    controllers[0].exists = is_controller_present(0);
    controllers[1].exists = is_controller_present(1);
    controllers[2].exists = is_controller_present(2);
    controllers[3].exists = is_controller_present(3);

    drives[0].controller_no = 0;
    drives[1].controller_no = 0;
    drives[2].controller_no = 1;
    drives[3].controller_no = 1;

    drives[0].ctrl_drive_no = 0;
    drives[1].ctrl_drive_no = 1;
    drives[2].ctrl_drive_no = 0;
    drives[3].ctrl_drive_no = 1;

    drives[0].exists = is_drive_present(0);
    drives[1].exists = is_drive_present(1);
    drives[2].exists = is_drive_present(2);
    drives[3].exists = is_drive_present(3);

    klog("Controller 0 is %s\n", controllers[0].exists ? "present" : "absent");
    klog("Controller 1 is %s\n", controllers[1].exists ? "present" : "absent");
    klog("Drive 0:0 is %s\n", drives[0].exists ? "present" : "absent");
    klog("Drive 0:1 is %s\n", drives[1].exists ? "present" : "absent");
    klog("Drive 1:0 is %s\n", drives[2].exists ? "present" : "absent");
    klog("Drive 1:1 is %s\n", drives[3].exists ? "present" : "absent");

    // it seems that drive 1:0 is present! (secondary master)
    memset(sector_buffer, 0, sizeof(sector_buffer));
    // read_block_lba28(2, 0);
    for (int i = 0; i < 64; i++) {
        klog("Sector %d\n", i);
        read_block_lba28(2, i);
        klog_hex16(sector_buffer, sizeof(sector_buffer));
    }
}

bool is_controller_present(int controller_no) {
    // but first detect if controllers are present before drive detection
    // write a magic value to the low lba port (0x1F3 or 0x173)
    // and read it back as the same, which shows the controller is there.
    outb(controllers[controller_no].io_port_base + 3, 0xAF);
    timer_pause_blocking(1);
    uint8_t value_back = inb(controllers[controller_no].io_port_base + 3);
    return value_back == 0xAF;
}

bool is_drive_present(int drive) {
    // then for drive detection,
    // first select the drive with the appropriate register (0x1F6 or 0x176)
    // wait for a little (4ms) and read the status register,
    // to detect the BUSY bit. 0xA0 for the first drive, 0xB0 for the second.
    // use 0xB0 instead of 0xA0 to test the second drive on the controller

    uint8_t controller_no = drives[drive].controller_no;

    uint8_t drive_value = drives[drive].ctrl_drive_no == 0 ? 0xA0 : 0xB0;
    outb(controllers[controller_no].io_port_base + 6, drive_value); 
    timer_pause_blocking(4); // wait 4ms
    uint8_t status = inb(controllers[controller_no].io_port_base + 7); // read the status port
    // see if the busy bit is set
    return status & 0x40;
}

void read_block_lba28(uint8_t drive, uint64_t block_num) {
    prepare_lba28_operation(drive, block_num, 0x20);
    wait_for_controller_to_be_ready(drive);
    transfer_disk_to_memory(drive);
}

void write_block_lba28(uint8_t drive, uint64_t block_num) {
    prepare_lba28_operation(drive, block_num, 0x30);
    wait_for_controller_to_be_ready(drive);
    transfer_memory_to_disk(drive);
}

void read_block_lba48(uint8_t drive, uint64_t block_num) {
    prepare_lba48_operation(drive, block_num, 0x24);
    wait_for_controller_to_be_ready(drive);
    transfer_disk_to_memory(drive);
}

void write_block_lba48(uint8_t drive, uint64_t block_num) {
    prepare_lba48_operation(drive, block_num, 0x34);
    wait_for_controller_to_be_ready(drive);
    transfer_memory_to_disk(drive);
}



static void prepare_lba28_operation(int drive, uint64_t block_num, uint8_t command) {
    uint16_t base_port = controllers[drives[drive].controller_no].io_port_base;
    uint16_t ctrl_drive_no = drives[drive].ctrl_drive_no;

    // to read a sector using LBA28
    // first null byte
    outb(base_port + 1, 0x00);
    // then sector count
    outb(base_port + 2, 0x01);
    // send the address in 3 bytes:
    outb(base_port + 3, (block_num >> 0) & 0xFF);
    outb(base_port + 4, (block_num >> 8) & 0xFF);
    outb(base_port + 5, (block_num >> 16) & 0xFF);
    // Send the drive indicator, some magic bits, and highest 4 bits of the block address to port 0x1F6
    outb(base_port + 6, 0xE0 | (ctrl_drive_no << 4) | ((block_num >> 24) & 0x0F));
    // then send the command 0x20 or 0x30 to port 1F7
    outb(base_port + 7, command);
}

static void prepare_lba48_operation(int drive, uint64_t block_num, uint8_t command) {
    uint16_t base_port = controllers[drives[drive].controller_no].io_port_base;
    uint16_t ctrl_drive_no = drives[drive].ctrl_drive_no;

    // to read a sector using LBA48:
    // send two null bytes first
    outb(base_port + 1, 0x00);
    outb(base_port + 1, 0x00);
    // send 16 bits sector count
    outb(base_port + 2, 0x00);
    outb(base_port + 2, 0x01);
    // Send bits 24-31 to port 0x1F3: 
    outb(base_port + 3, ((block_num >> 24) & 0xFF));
    // Send bits 0-7 to port 0x1F3: 
    outb(base_port + 3, (block_num & 0xFF));
    // Send bits 32-39 to port 0x1F4: 
    outb(base_port + 4, ((block_num >> 32) & 0xFF));
    // Send bits 8-15 to port 0x1F4: 
    outb(base_port + 4, ((block_num >> 8) & 0xFF));
    // Send bits 40-47 to port 0x1F5: 
    outb(base_port + 5, ((block_num >> 40) & 0xFF));
    // Send bits 16-23 to port 0x1F5: 
    outb(base_port + 5, ((block_num >> 16) & 0xFF));
    // Send the drive indicator and some magic bits to port 0x1F6
    outb(base_port + 6, 0x40 | (ctrl_drive_no << 4));
    // Send the command 0x24 or 0x34 to port 0x1F7
    outb(base_port + 7, command);

}

static void wait_for_controller_to_be_ready(int drive) {
    // Once you've done all this, you just have to wait for the drive to signal that it's ready
    uint16_t port_base = controllers[drives[drive].controller_no].control_port_base;
    while (!(inb(port_base + 7) & 0x08)) {
        ;
    }
}

static void transfer_disk_to_memory(int drive) {
    // And then read/write your data from/to port 0x1F0 - for read:
    uint16_t port_base = controllers[drives[drive].controller_no].control_port_base;
    for (int i = 0; i < 256; i++) {
        uint16_t word = inw(port_base);
        sector_buffer[i * 2] = word & 0xFF;
        sector_buffer[i * 2 + 1] = (word >> 8) & 0xFF;
    }
}
static void transfer_memory_to_disk(int drive) {
    // And then read/write your data from/to port 0x1F0 - for write:
    uint16_t port_base = controllers[drives[drive].controller_no].control_port_base;
    for (int i = 0; i < 256; i++) {
        uint16_t word = 
            (uint16_t)(sector_buffer[i * 2]) | 
            ((uint16_t)(sector_buffer[i * 2 + 1] << 8));
        outw(port_base, word);
    }
}
