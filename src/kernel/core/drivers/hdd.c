#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "../cpu.h"
#include "../string.h"
#include "screen.h"
#include "timer.h"
#include "../klog.h"


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

// these for the io_port
#define DATA_REGISTER           0
#define FEATURE_REGISTER        1
#define ERROR_REGISTER          1 // on reading
#define SECTOR_COUNT_REGISTER   2
#define LBA_LOW_REGISTER        3
#define LBA_MID_REGISTER        4
#define LBA_HIGH_REGISTER       5
#define DRIVE_SELECT_REGISTER   6
#define COMMAND_REGISTER        7
#define STATUS_REGISTER         7 // on reading

#define CMD_IDENTIFY      0xEC
#define CMD_READ_LBA28    0x00
#define CMD_WRITE_LBA28   0x00
#define CMD_READ_LBA48    0x00
#define CMD_WRITE_LBA48   0x00


#define ERR      0x01  // bit 0 from status register
#define DRQ      0x08  // bit 3 from status register, drive has data to send or receive
#define DF       0x20  // bit 5 from status register, drive fault error
#define RDY      0x40  // bit 6, when disks have spun down
#define BSY      0x80  // bit 7 from status register


#define SRST     0x04  // written to the control register

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
    bool is_atapi;
    bool is_sata;
};
typedef struct drive_info drive_info_t;

// read/write in LBA (Linear Block Address) mode, each block is 512 bytes long.
// most drives support LBA28 (28 bits for block number)
// not all support LBA48 (48 bits for block number)
uint8_t sector_buffer[512];
controller_info_t controllers[] = {
    // io-port, control-port, exists
    { 0x1F0, 0x3F6, 0 },
    { 0x170, 0x376, 0 },
};
drive_info_t drives[] = {
    // ctrl-no, drive-no, exists, atapi, sata
    { 0, 0, 0, 0, 0},
    { 0, 1, 0, 0, 0},
    { 1, 0, 0, 0, 0},
    { 1, 1, 0, 0, 0},
};

bool is_drive_present(int drive);
void soft_reset_controller(int controller_no);
void identify_drive(uint8_t drive_idx);
static void prepare_lba28_operation(int drive, uint64_t block_num, uint8_t command);
static void prepare_lba48_operation(int drive, uint64_t block_num, uint8_t command);
static void wait_for_controller_to_be_ready(int drive);
static void transfer_256_words_disk_to_memory(int drive);
static void transfer_256_words_memory_to_disk(int drive);
void read_block_lba28(uint8_t drive, uint64_t block_num);
void write_block_lba28(uint8_t drive, uint64_t block_num);
void read_block_lba48(uint8_t drive, uint64_t block_num);
void write_block_lba48(uint8_t drive, uint64_t block_num);

void test_hdd() {
    for (int i = 0; i < (int)(sizeof(controllers) / sizeof(controllers[0])); i++) {
        soft_reset_controller(i);
        //controllers[i].exists = is_controller_present(i);
        //klog("Controller #%d is %s\n", i, controllers[i].exists ? "present" : "absent");
    }
    for (int i = 0; i < (int)(sizeof(drives) / sizeof(drives[0])); i++) {
        // drives[i].exists = is_drive_present(i);
        identify_drive(i);
        // klog("Drive #%d is %s\n", i, drives[i].exists ? "present" : "absent");
    }

    // for (int d = 0; d < (int)(sizeof(drives) / sizeof(drives[0])); d++) {
    //     if (drives[d].exists) {
    //         klog("---- Drive #%d ----\n", d);
    //         // it seems that drive 1:0 is present! (secondary master)
    //         memset(sector_buffer, 0, sizeof(sector_buffer));
    //         for (int s = 0; s < 1; s++) {
    //             // klog("Sector %d\n", s);
    //             read_block_lba28(d, s);
    //             // klog_hex16((char *)sector_buffer, sizeof(sector_buffer), s * 512);
    //             klog_hex16((char *)sector_buffer, 32, s * 512);
    //         }
    //     }
    // }
}

void controller_status(int controller_no) {
    uint8_t status = inb(controllers[controller_no].control_port_base);
    klog("Controller %d status  reg x%02x: %s%s%s%s%s%s%s%s\n",
        controller_no,
        status,
        (status & 0x01) ? "ERR " : "",
        (status & 0x02) ? "IDX " : "",
        (status & 0x04) ? "CORR " : "",
        (status & 0x08) ? "DRQ " : "",
        (status & 0x10) ? "SRV " : "",
        (status & 0x20) ? "DF " : "",
        (status & 0x40) ? "RDY " : "",
        (status & 0x80) ? "BSY " : ""
    );
    status = inb(controllers[controller_no].control_port_base + 1);
    klog("Controller %d address reg x%02x: %s%s%s%s%s%s%s%s\n",
        controller_no,
        status,
        (status & 0x01) ? "DS0 " : "",
        (status & 0x02) ? "DS1 " : "",
        (status & 0x04) ? "HS0 " : "",
        (status & 0x08) ? "HS1 " : "",
        (status & 0x10) ? "HS2 " : "",
        (status & 0x20) ? "HS3 " : "",
        (status & 0x40) ? "WTG " : "",
        (status & 0x80) ? "n/a " : ""
    );
}

bool wait_controller_not_busy(int controller_no) {
    uint64_t started = timer_get_uptime_msecs();
    while (true) {
        uint8_t status = inb(controllers[controller_no].control_port_base);
        if ((status & BSY) == 0 && (status & DRQ) == 0)
            return true;
        if (timer_get_uptime_msecs() - started > 50)
            return false;
    }
}

bool wait_controller_ready(int controller_no) {
    uint64_t started = timer_get_uptime_msecs();
    while (true) {
        uint8_t status = inb(controllers[controller_no].control_port_base);
        if (status & RDY)
            return true;
        if (timer_get_uptime_msecs() - started > 50)
            return false;
    }
}

void soft_reset_controller(int controller_no) {
    controller_status(controller_no);

    klog("Resetting controller %d...\n", controller_no);
    outb(controllers[controller_no].control_port_base, 0);
    inb(controllers[controller_no].io_port_base); // pause for 30usec

    // to do a reset, send the RST bit for 5usecs on the control port
    // the master drive is automatically selected

    outb(controllers[controller_no].control_port_base, SRST);
    // timer_pause_blocking(1);
    inb(controllers[controller_no].io_port_base); // pause for 30usec
    outb(controllers[controller_no].control_port_base, 0);

    if (!wait_controller_not_busy(controller_no)) {
        klog("Controller failed clearing BSY after reset\n");
        return;
    }
    if (!wait_controller_ready(controller_no)) {
        klog("Controller failed getting RDY after reset\n");
        return;
    }
    controller_status(controller_no);
}

void send_command(uint8_t drive_idx, uint8_t command) {
    // estimated that each IO port read takes 30 usecs
    
    uint8_t cno = drives[drive_idx].controller_no;
    controller_info_t *ctrlr = &controllers[cno];
    wait_controller_not_busy(cno);
    wait_controller_ready(cno);

    // drive select first, 
    uint8_t drive_value = drives[drive_idx].ctrl_drive_no == 0 ? 0xA0 : 0xB0;
    outb(ctrlr->io_port_base + DRIVE_SELECT_REGISTER, drive_value); 

    wait_controller_not_busy(cno);
}

void identify_drive(uint8_t drive_idx) {

    uint8_t cno = drives[drive_idx].controller_no;
    controller_info_t *ctrlr = &controllers[cno];

    // reading the alternate status register 15 times, to delay before selecting a new drive
    // ideally, the software should remember the last drive we selected
    // and only pause when we change drives. but that's ok for now.
    uint8_t status, p1, p2;


    klog("Identifying drive %d...\n", drive_idx);
    wait_controller_not_busy(cno);
    wait_controller_ready(cno);

    uint8_t drive_value = drives[drive_idx].ctrl_drive_no == 0 ? 0xA0 : 0xB0;
    outb(ctrlr->io_port_base + DRIVE_SELECT_REGISTER, drive_value); 
    for (int i = 0; i < 15; i++)
        status = inb(ctrlr->control_port_base);
    klog("After drive select + delay, ctrl_port status is %08bb\n", drive_idx);
    outb(ctrlr->io_port_base + LBA_LOW_REGISTER, 0); 
    outb(ctrlr->io_port_base + LBA_MID_REGISTER, 0); 
    outb(ctrlr->io_port_base + LBA_HIGH_REGISTER, 0); 
    outb(ctrlr->io_port_base + COMMAND_REGISTER, CMD_IDENTIFY); 

    status = inb(ctrlr->io_port_base + STATUS_REGISTER); 
    if (status == 0) {
        // drive does not exist
        klog("Drive does not exist\n");
        return;
    }
    status = inb(ctrlr->io_port_base + STATUS_REGISTER);
    klog("Status register after identify %08bb\n", status);

    // poll the status, until bit 7 (BSY) clears
    klog("Polling to overcome BSY...\n");
    uint64_t started = timer_get_uptime_msecs();
    while (true) {
        status = inb(ctrlr->io_port_base + STATUS_REGISTER);
        if ((status & BSY) == 0)
            break;
        if (timer_get_uptime_msecs() - started > 50) {
            klog("Timed out waiting for BSY to clear, after 100 msecs...\n");
            return;
        }
    }

    // Because of some ATAPI drives that do not follow spec, at this point 
    // you need to check the LBAmid and LBAhi ports (0x1F4 and 0x1F5) 
    // to see if they are non-zero. If so, the drive is not ATA, and 
    /// you should stop polling. 
    p1 = inb(ctrlr->io_port_base + LBA_MID_REGISTER);
    p2 = inb(ctrlr->io_port_base + LBA_HIGH_REGISTER);
    if (p1 != 0 || p2 != 0) {
        bool is_atapi = p1 == 0x14 && p2 == 0xEB;
        bool is_sata = p1 == 0x3C && p2 == 0xC3;
        if (is_atapi)
            klog("ATAPI device detected, stopping\n");
        if (is_sata)
            klog("SATA device detected, stopping\n");
        if (!is_atapi && !is_sata)
            klog("Unknown non-ATA device detected, stopping\n");
        return;
    }

    // Otherwise, continue polling one of the Status ports until 
    // bit 3 (DRQ, value = 8) sets, or until bit 0 (ERR, value = 1) sets.
    klog("Waiting for either DRQ or ERR\n");
    while (true) {
        status = inb(ctrlr->io_port_base + STATUS_REGISTER);
        if (status & ERR) {
            klog("Error detected, aborting\n");
            return;
        }
        if (status & DRQ)
            break;
    }

    klog("DRQ detected, transfering...\n");
    transfer_256_words_disk_to_memory(drive_idx);
    klog_hex16((char *)sector_buffer, 64, 0);
}



void read_block_lba28(uint8_t drive_idx, uint64_t block_num) {
    prepare_lba28_operation(drive_idx, block_num, 0x20);
    wait_for_controller_to_be_ready(drive_idx);
    transfer_256_words_disk_to_memory(drive_idx);
}

void write_block_lba28(uint8_t drive_idx, uint64_t block_num) {
    prepare_lba28_operation(drive_idx, block_num, 0x30);
    wait_for_controller_to_be_ready(drive_idx);
    transfer_256_words_memory_to_disk(drive_idx);
}

void read_block_lba48(uint8_t drive_idx, uint64_t block_num) {
    prepare_lba48_operation(drive_idx, block_num, 0x24);
    wait_for_controller_to_be_ready(drive_idx);
    transfer_256_words_disk_to_memory(drive_idx);
}

void write_block_lba48(uint8_t drive_idx, uint64_t block_num) {
    prepare_lba48_operation(drive_idx, block_num, 0x34);
    wait_for_controller_to_be_ready(drive_idx);
    transfer_256_words_memory_to_disk(drive_idx);
}



static void prepare_lba28_operation(int drive_idx, uint64_t block_num, uint8_t command) {
    uint16_t base_port = controllers[drives[drive_idx].controller_no].io_port_base;
    uint16_t ctrl_drive_no = drives[drive_idx].ctrl_drive_no;

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

static void prepare_lba48_operation(int drive_idx, uint64_t block_num, uint8_t command) {
    uint16_t base_port = controllers[drives[drive_idx].controller_no].io_port_base;
    uint16_t ctrl_drive_no = drives[drive_idx].ctrl_drive_no;

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

static void wait_for_controller_to_be_ready(int drive_idx) {
    // Once you've done all this, you just have to wait for the drive to signal that it's ready
    uint16_t port_base = controllers[drives[drive_idx].controller_no].control_port_base;
    while (!(inb(port_base + 7) & 0x08)) {
        ;
    }
}

static void transfer_256_words_disk_to_memory(int drive_idx) {
    // And then read/write your data from/to port 0x1F0 - for read:
    uint16_t port_base = controllers[drives[drive_idx].controller_no].control_port_base;
    for (int i = 0; i < 256; i++) {
        uint16_t word = inw(port_base);
        sector_buffer[i * 2] = word & 0xFF;
        sector_buffer[i * 2 + 1] = (word >> 8) & 0xFF;
    }
}
static void transfer_256_words_memory_to_disk(int drive_idx) {
    // And then read/write your data from/to port 0x1F0 - for write:
    uint16_t port_base = controllers[drives[drive_idx].controller_no].control_port_base;
    for (int i = 0; i < 256; i++) {
        uint16_t word = 
            (uint16_t)(sector_buffer[i * 2]) | 
            ((uint16_t)(sector_buffer[i * 2 + 1] << 8));
        outw(port_base, word);
    }
}
