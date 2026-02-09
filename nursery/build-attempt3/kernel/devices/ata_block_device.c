#include "ata_block_device.h"
#include "../drivers/pci.h"
#include "../memory/kheap.h"
#include "../utils/logger.h"
#include "../drivers/timer.h"
#include "../klib/string.h"
#include "../include/bits.h"
#include "../misc/cpu.h"


MODULE("ATA_BLK", LOG_LEVEL_WARN);

// Data structures copied from the old ata.c driver

// for primary / secondary channel
struct ide_channel {
   uint16_t base_port;  // I/O Base.
   uint16_t ctrl_port;  // Control Base
   uint16_t bus_master_ide; // Bus Master IDE
   uint8_t  nIEN;  // nIEN (No Interrupt);
};

// for the four drives
struct ide_drive {
   uint8_t  detected;     // 0 (Empty) or 1 (This drive really exists).
   uint8_t  channel_no;   // 0 (Primary channel) or 1 (Secondary channel).
   uint8_t  master_slave; // 0 (Master drive) or 1 (Slave drive).
   uint16_t type;         // 0: ATA, 1:ATAPI.
   uint32_t size;         // size in Sectors.
   char  model[41];    // model in string.
   struct ide_channel *channel;
};

// private data for the block device
typedef struct {
    char *name;
    struct ide_drive *drive;
} ata_priv_t;


// at most 4 drives (primary master/slave, secondary master/slave)
#define MAX_ATA_DEVICES 4
static block_device_t *ata_devices[MAX_ATA_DEVICES] = {0};
static int ata_device_count = 0;

extern pci_device_t *pci_devices_list;

// --- Register I/O ---
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C

static void write_register(struct ide_channel *channel, uint8_t reg, uint8_t data) {
    if (reg > 0x07 && reg < 0x0C)
        write_register(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);
    if (reg < 0x08)
        outb(channel->base_port + reg, data);
    else if (reg < 0x0E)
        outb(channel->ctrl_port + (reg - 0x0A), data);
    if (reg > 0x07 && reg < 0x0C)
        write_register(channel, ATA_REG_CONTROL, channel->nIEN);
}

static uint8_t read_register(struct ide_channel *channel, uint8_t reg) {
    uint8_t data;
    if (reg > 0x07 && reg < 0x0C)
        write_register(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);
    if (reg < 0x08)
        data = inb(channel->base_port + reg);
    else if (reg < 0x0E)
        data = inb(channel->ctrl_port + (reg - 0x0A));
    if (reg > 0x07 && reg < 0x0C)
        write_register(channel, ATA_REG_CONTROL, channel->nIEN);
    return data;
}

static void read_buffer(struct ide_channel *channel, uint8_t reg, void *buffer, uint32_t quads) {
    if (reg > 0x07 && reg < 0x0C)
        write_register(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);
    asm volatile ("rep insl" : : "c"(quads), "d"(channel->base_port + reg), "D"(buffer));
    if (reg > 0x07 && reg < 0x0C)
        write_register(channel, ATA_REG_CONTROL, channel->nIEN);
}


// --- ATA commands and status ---
#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Index
#define ATA_SR_ERR     0x01    // Error

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_IDENTIFY          0xEC


static uint8_t poll(struct ide_channel *channel) {
    for(int i=0; i<4; i++)
        read_register(channel, ATA_REG_ALTSTATUS);

    while (read_register(channel, ATA_REG_STATUS) & ATA_SR_BSY);

    uint8_t status = read_register(channel, ATA_REG_STATUS);
    if (status & ATA_SR_ERR) return 1;
    if (status & ATA_SR_DF) return 1;
    if (!(status & ATA_SR_DRQ)) return 1;
    return 0;
}


// --- Block device ops ---

static size_t ata_total_blocks(block_device_t *dev) {
    ata_priv_t *priv = (ata_priv_t *)dev->priv_data;
    return priv->drive->size;
}

static size_t ata_block_size(block_device_t *dev) {
    (void)dev;
    return 512;
}

static error_t ata_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buffer) {
    ata_priv_t *priv = (ata_priv_t *)dev->priv_data;
    struct ide_drive *drive = priv->drive;
    struct ide_channel *channel = drive->channel;
    uint8_t cmd = ATA_CMD_READ_PIO;

    // LBA48 not implemented, this driver only supports LBA28
    if (lba > 0x0FFFFFFF) return ERR_INVALID_ARGS;

    write_register(channel, ATA_REG_HDDEVSEL, 0xE0 | (drive->master_slave << 4) | ((lba >> 24) & 0x0F));
    write_register(channel, ATA_REG_FEATURES, 0x00);
    write_register(channel, ATA_REG_SECCOUNT0, count);
    write_register(channel, ATA_REG_LBA0, (uint8_t)lba);
    write_register(channel, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    write_register(channel, ATA_REG_LBA2, (uint8_t)(lba >> 16));
    write_register(channel, ATA_REG_COMMAND, cmd);

    for (uint32_t i = 0; i < count; i++) {
        if (poll(channel)) return ERR_IO_ERROR;
        read_buffer(channel, ATA_REG_DATA, (char*)buffer + i * 512, 128);
    }

    return OK;
}

static error_t ata_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buffer) {
    ata_priv_t *priv = (ata_priv_t *)dev->priv_data;
    struct ide_drive *drive = priv->drive;
    struct ide_channel *channel = drive->channel;
    uint8_t cmd = ATA_CMD_WRITE_PIO;

    // LBA48 not implemented
    if (lba > 0x0FFFFFFF) return ERR_INVALID_ARGS;

    write_register(channel, ATA_REG_HDDEVSEL, 0xE0 | (drive->master_slave << 4) | ((lba >> 24) & 0x0F));
    write_register(channel, ATA_REG_FEATURES, 0x00);
    write_register(channel, ATA_REG_SECCOUNT0, count);
    write_register(channel, ATA_REG_LBA0, (uint8_t)lba);
    write_register(channel, ATA_REG_LBA1, (uint8_t)(lba >> 8));
    write_register(channel, ATA_REG_LBA2, (uint8_t)(lba >> 16));
    write_register(channel, ATA_REG_COMMAND, cmd);

    for (uint32_t i = 0; i < count; i++) {
        if (poll(channel)) return ERR_IO_ERROR;
        asm volatile ("rep outsl" : : "c"(128), "d"(channel->base_port), "S"((char*)buffer + i * 512));
    }

    return OK;
}

static error_t ata_flush(block_device_t *dev) {
    (void)dev;
    return OK;
}

static const char *ata_name(block_device_t *dev) {
    ata_priv_t *priv = (ata_priv_t *)dev->priv_data;
    return priv->name;
}

static void ata_destroy(block_device_t *dev) {
    if(dev) {
        if(dev->priv_data) {
            ata_priv_t *priv = (ata_priv_t *)dev->priv_data;
            kfree(priv->name);
            kfree(priv->drive); // Free the ide_drive structure
            kfree(priv);
        }
        kfree(dev);
    }
}

static struct block_device_ops ata_ops = {
    .name = ata_name,
    .total_blocks = ata_total_blocks,
    .block_size = ata_block_size,
    .read = ata_read,
    .write = ata_write,
    .flush = ata_flush,
    .destroy = ata_destroy
};


// --- Initialization ---

void init_ata_block_devices() {
    pci_device_t *pci_dev = pci_devices_list;
    while(pci_dev) {
        // Class 0x01 (Mass Storage Controller), Subclass 0x01 (IDE Controller)
        if (pci_dev->config.class_type == 0x01 && pci_dev->config.sub_class == 0x01) {
            break;
        }
        pci_dev = pci_dev->next;
    }

    if (!pci_dev) {
        log_warn("No IDE controller found");
        return;
    }

    log_info("Found IDE controller");

    // Allocate channels on the heap to avoid stack overflow issues for large structures
    struct ide_channel *channels = kmalloc(sizeof(struct ide_channel) * 2);
    if (!channels) {
        log_error("Failed to allocate memory for IDE channels");
        return;
    }

    channels[0].base_port = pci_dev->config.headers.h00.bar0 ? (pci_dev->config.headers.h00.bar0 & 0xFFFFFFFC) : 0x1F0;
    channels[0].ctrl_port = pci_dev->config.headers.h00.bar1 ? (pci_dev->config.headers.h00.bar1 & 0xFFFFFFFC) : 0x3F4;
    channels[1].base_port = pci_dev->config.headers.h00.bar2 ? (pci_dev->config.headers.h00.bar2 & 0xFFFFFFFC) : 0x170;
    channels[1].ctrl_port = pci_dev->config.headers.h00.bar3 ? (pci_dev->config.headers.h00.bar3 & 0xFFFFFFFC) : 0x374;

    // Disable IRQs
    write_register(&channels[0], ATA_REG_CONTROL, 2);
    write_register(&channels[1], ATA_REG_CONTROL, 2);


    for (int i=0; i<2; i++) { // Channel (Primary/Secondary)
        for (int j=0; j<2; j++) { // Drive (Master/Slave)
            struct ide_drive *drive = kmalloc(sizeof(struct ide_drive));
            if (!drive) {
                log_error("Failed to allocate memory for IDE drive");
                continue;
            }
            drive->channel = &channels[i];
            drive->channel_no = i;
            drive->master_slave = j;
            drive->detected = 0; // Assume not detected until proven otherwise

            write_register(drive->channel, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
            timer_pause_blocking(1);

            write_register(drive->channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            timer_pause_blocking(1);
            
            if (read_register(drive->channel, ATA_REG_STATUS) == 0) { // No device
                kfree(drive);
                continue; 
            }

            uint8_t status;
            while(1) {
                status = read_register(drive->channel, ATA_REG_STATUS);
                if ((status & ATA_SR_ERR)) {
                    log_debug("ERR bit set, not an ATA drive");
                    kfree(drive);
                    goto next_drive;
                }
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
            }

            drive->detected = 1;
            uint16_t id_buf[256];
            read_buffer(drive->channel, ATA_REG_DATA, id_buf, 128);
            
            drive->size = *(uint32_t*)(id_buf + 60); // LBA28 sectors
            if (drive->size == 0) { // Try LBA48 if LBA28 is zero
                drive->size = *(uint32_t*)(id_buf + 100); // LBA48 sectors (low 32 bits)
            }


            for(int k = 0; k < 40; k += 2) {
                drive->model[k] = id_buf[27 + k/2] >> 8;
                drive->model[k + 1] = id_buf[27 + k/2] & 0xFF;
            }
            drive->model[40] = 0; // Null-terminate
            // Trim trailing spaces
            for (int k = 39; k >= 0 && drive->model[k] == ' '; k--) {
                drive->model[k] = '\0';
            }


            char name_buffer[64];
            sprintfn(name_buffer, sizeof(name_buffer), "ATA %s %s (%s)",
                drive->channel_no == 0 ? "Primary" : "Secondary",
                drive->master_slave == 0 ? "Master" : "Slave",
                drive->model
            );

            ata_priv_t *priv = kmalloc(sizeof(ata_priv_t));
            if (!priv) {
                log_error("Failed to allocate memory for ATA private data");
                kfree(drive);
                continue;
            }
            priv->name = strdup(name_buffer);
            if (!priv->name) {
                log_error("Failed to duplicate ATA device name");
                kfree(drive);
                kfree(priv);
                continue;
            }
            priv->drive = drive;

            block_device_t *dev = kmalloc(sizeof(block_device_t));
            if (!dev) {
                log_error("Failed to allocate memory for block device");
                kfree(drive);
                kfree(priv->name);
                kfree(priv);
                continue;
            }
            dev->priv_data = priv;
            dev->ops = &ata_ops;

            log_info("Found block device: %s, size: %lu sectors", priv->name, drive->size);

            if (ata_device_count < MAX_ATA_DEVICES) {
                ata_devices[ata_device_count++] = dev;
            } else {
                log_warn("Maximum ATA devices reached, skipping %s", priv->name);
                ata_destroy(dev); // Free newly allocated resources
            }


        next_drive:;
        }
    }
    kfree(channels); // Free the channels allocated on the heap
}

int get_ata_block_device_count() {
    return ata_device_count;
}

error_t create_ata_block_device(int index, block_device_t **result) {
    if (index < 0 || index >= ata_device_count) {
        *result = NULL;
        return ERR_INVALID_ARGS;
    }
    *result = ata_devices[index];
    return OK;
}
