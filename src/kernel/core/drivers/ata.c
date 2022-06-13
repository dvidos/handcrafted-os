#include <stddef.h>
#include <stdbool.h>
#include <bits.h>
#include <stdbool.h>
#include <stdint.h>
#include <cpu.h>
#include <klib/string.h>
#include <drivers/screen.h>
#include <drivers/timer.h>
#include <drivers/pci.h>
#include <devices/storage_dev.h>
#include <memory/kheap.h>
#include <memory/physmem.h>
#include <klog.h>


// heavily influenced (read: copied) from here https://wiki.osdev.org/PCI_IDE_Controller
// and here: https://github.com/HaontCorporation/os365/blob/master/hdd.h

static volatile uint8_t ide_irq_invoked = 0; // supposedly we turn this to 1 in interrupt handler

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
   uint16_t signature;    // drive signature
   uint16_t capabilities; // Features.
   uint32_t command_sets; // Command Sets Supported.
   uint32_t size;         // size in Sectors.
   uint8_t  model[41];    // model in string.
};


// for use with the pci device
struct pci_dev_driver_data {
    struct ide_channel channels[2];
    struct ide_drive drives[4];
};

// for use with storage_dev
struct storage_dev_driver_data {
    // 0-3, an index into the drives array
    char drive_no;
};


// the Command/Status port return values for channel
#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Index
#define ATA_SR_ERR     0x01    // Error

// the Features/Error port return values
#define ATA_ER_BBK      0x80    // Bad block
#define ATA_ER_UNC      0x40    // Uncorrectable data
#define ATA_ER_MC       0x20    // Media changed
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // Media change request
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

// commands for the Command/Status port
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

// IDENTITY commands return 512 bytes of data, this helps parse it.
#define ATA_IDENT_DEVICETYPE     0
#define ATA_IDENT_CYLINDERS      2
#define ATA_IDENT_HEADS          6
#define ATA_IDENT_SECTORS       12
#define ATA_IDENT_SERIAL        20
#define ATA_IDENT_MODEL         54
#define ATA_IDENT_CAPABILITIES  98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

// useful for when selecting a drive
#define IDE_ATA        0x00
#define IDE_ATAPI      0x01
#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

// Registers definitions. Some values duplicated for better naming
// See read_register()/write_register() for usage
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
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

// channels
#define PRIMARY_CHANNEL    0x00
#define SECONDARY_CHANNEL  0x01
 
// directions
#define ATA_READ      0x00
#define ATA_WRITE     0x01

// custom errors
#define IDE_NO_ERR                         0
#define IDE_ERR_DEVICE_FAULT               1
#define IDE_ERR_STATUS_ERROR               2
#define IDE_ERR_NO_DATA_REQ                3
#define IDE_ERR_ADDR_MARK_NOT_FOUND        4
#define IDE_ERR_NO_MEDIA                   5
#define IDE_ERR_CMD_ABORTED                6
#define IDE_ERR_ID_MARK_NOT_FOUND          7
#define IDE_ERR_UNCORRECTABLE_DATA_ERROR   8
#define IDE_ERR_BAD_SECTORS                9
#define IDE_ERR_DRIVE_NOT_FOUND           10
#define IDE_ERR_INVALID_ADDRESS           11
#define IDE_ERR_READ_ONLY                 12




// write a value to a register of a channel, e.g. write_register(PRIMARY_CHANNEL, ATA_REG_COMMAND, ATA_CMD_WRITE_PIO)
static void write_register(struct ide_channel *channel, uint8_t reg_no, uint8_t value) {

    if (reg_no > 0x07 && reg_no < 0x0C)
        write_register(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);

    if (reg_no < 0x08)
        outb(channel->base_port  + reg_no - 0x00, value);
    else if (reg_no < 0x0C)
        outb(channel->base_port  + reg_no - 0x06, value);
    else if (reg_no < 0x0E)
        outb(channel->ctrl_port  + reg_no - 0x0A, value);
    else if (reg_no < 0x16)
        outb(channel->bus_master_ide + reg_no - 0x0E, value);

    if (reg_no > 0x07 && reg_no < 0x0C)
        write_register(channel, ATA_REG_CONTROL, channel->nIEN);
}


// read a register in a channel (e.g. read_register(PRIMARY_CHANNEL, ATA_REG_STATUS))
static uint8_t read_register(struct ide_channel *channel, uint8_t reg_no) {
    uint8_t result = 0;

    if (reg_no > 0x07 && reg_no < 0x0C)
        write_register(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);

    if (reg_no < 0x08)
        result = inb(channel->base_port + reg_no - 0x00);
    else if (reg_no < 0x0C)
        result = inb(channel->base_port  + reg_no - 0x06);
    else if (reg_no < 0x0E)
        result = inb(channel->ctrl_port  + reg_no - 0x0A);
    else if (reg_no < 0x16)
        result = inb(channel->bus_master_ide + reg_no - 0x0E);

    if (reg_no > 0x07 && reg_no < 0x0C)
        write_register(channel, ATA_REG_CONTROL, channel->nIEN);

    return result;
}

// the following reads a number of dwords in PIO mode, useful for IDENTITY
// the buffer can be a char buffer, but we read 32-bit values from the ports
static void read_buffer_pio(struct ide_channel *channel, uint8_t reg_no, uint32_t *buffer, uint32_t dwords) {
    uint32_t result = 0;

    if (reg_no > 0x07 && reg_no < 0x0C)
        write_register(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);

    while (dwords-- > 0) {
        if (reg_no < 0x08)
            result = inl(channel->base_port + reg_no - 0x00);
        else if (reg_no < 0x0C)
            result = inl(channel->base_port  + reg_no - 0x06);
        else if (reg_no < 0x0E)
            result = inl(channel->ctrl_port  + reg_no - 0x0A);
        else if (reg_no < 0x16)
            result = inl(channel->bus_master_ide + reg_no - 0x0E);
        
        *buffer++ = result;
    }

    if (reg_no > 0x07 && reg_no < 0x0C)
        write_register(channel, ATA_REG_CONTROL, channel->nIEN);
}

// after a command has been issued, where's a standard follow up.
// this method polls to see successful result or not
static uint8_t poll(struct ide_channel *channel, bool advanced_check) {

    // delay 400 nanosecond for BSY to be set:
    // Reading the Alternate Status port wastes 100ns; loop four times.
    for(int i = 0; i < 4; i++)
        read_register(channel, ATA_REG_ALTSTATUS); 

    // wait for BSY to be cleared:
    while (read_register(channel, ATA_REG_STATUS) & ATA_SR_BSY)
        ;

    // this is helpful for some commands
    if (advanced_check) {
        uint8_t state = read_register(channel, ATA_REG_STATUS); 
        klog_debug("IDE poll, state=0x%02x (%08bb)", state, state);

        // check for error
        if (state & ATA_SR_ERR)
            return IDE_ERR_STATUS_ERROR;

        // check for device fault
        if (state & ATA_SR_DF)
            return IDE_ERR_DEVICE_FAULT;

        // timer_pause_blocking(1);
        // BSY = 0; DF = 0; ERR = 0, so check for DRQ (should be set)
        if ((state & ATA_SR_DRQ) == 0)
            return IDE_ERR_NO_DATA_REQ;
    }

    return 0;
}

/* drive is the drive number which can be from 0 to 3.
   lba is the LBA address which allows us to access disks up to 2TB.
   numsects is the number of sectors to be read, it is a char, as reading more than 256 sector immediately may performance issues. If numsects is 0, the ATA controller will know that we want 256 sectors.
   selector is the segment selector to read from, or write to.
   edi is the offset in that segment. (the memory address for the data buffer)
*/
static uint8_t ata_rw_operation(struct pci_dev_driver_data *data, uint8_t direction, uint8_t drive_no, uint32_t lba, uint8_t numsects, uint16_t data_seg, uint32_t data_ptr) {
    klog_trace("ata_rw_operation(dir=%s, drive_no=%d, lba=%d, num_sects=%d, data_seg=0x%x, data_ptr=0x%x)", 
        direction == ATA_WRITE ? "WRITE" : "READ", drive_no, lba, numsects, data_seg, data_ptr);

    struct ide_drive   *drive   = &data->drives[drive_no];
    struct ide_channel *channel = &data->channels[drive->channel_no];

    uint8_t  lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */, dma /* 0: No DMA, 1: DMA */, cmd;
    uint8_t  lba_io[6];
    uint32_t bus        = channel->base_port; // Bus Base, like 0x1F0 which is also data port.
    uint32_t slavebit   = drive->master_slave;
    uint32_t words      = 256; // Almost every ATA drive has a sector-size of 512-byte.
    uint16_t cyl;
    uint8_t  head, sect, err;

    // We don't need IRQs, so we should disable it to prevent problems from happening.
    // We said before that if bit 1 of the Control Register (which is called nIEN bit), is set, 
    // no IRQs will be invoked from any drives on this channel, either master or slave.
    write_register(channel, ATA_REG_CONTROL, channel->nIEN = (ide_irq_invoked = 0x0) + 0x02);

    // select one from LBA28, LBA48 or CHS
    if (lba >= 0x10000000) { 
        // Sure drive should support LBA in this case, or you are giving a wrong LBA.
        // LBA48:
        lba_mode  = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
    } else if (data->drives[drive_no].capabilities & 0x200)  { // drive supports LBA?
        // LBA28:
        lba_mode  = 1;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0; // These Registers are not used here.
        lba_io[4] = 0; // These Registers are not used here.
        lba_io[5] = 0; // These Registers are not used here.
        head      = (lba & 0xF000000) >> 24;
    } else {
        // CHS:
        lba_mode  = 0;
        sect      = (lba % 63) + 1;
        cyl       = (lba + 1  - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xFF;
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head      = (lba + 1  - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
    }

    // (II) See if drive supports DMA or not;
    dma = 0; // We don't support DMA

    // Wait if the drive is busy
    while (read_register(channel, ATA_REG_STATUS) & ATA_SR_BSY)
        ;

    /*
        The HDDDEVSEL register now looks like this:
        Bits 0 : 3: Head Number for CHS.
        Bit 4: Slave Bit. (0: Selecting Master drive, 1: Selecting Slave drive).
        Bit 5: Obsolete and isn't used, but should be set.
        Bit 6: LBA (0: CHS, 1: LBA).
        Bit 7: Obsolete and isn't used, but should be set.
    */

    // Select drive from the controller
    if (lba_mode == 0)
        write_register(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head); // drive & CHS.
    else
        write_register(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head); // drive & LBA   

    // Write Parameters;
    if (lba_mode == 2) {
        write_register(channel, ATA_REG_SECCOUNT1, 0);
        write_register(channel, ATA_REG_LBA3, lba_io[3]);
        write_register(channel, ATA_REG_LBA4, lba_io[4]);
        write_register(channel, ATA_REG_LBA5, lba_io[5]);
    }
    write_register(channel, ATA_REG_SECCOUNT0, numsects);
    write_register(channel, ATA_REG_LBA0, lba_io[0]);
    write_register(channel, ATA_REG_LBA1, lba_io[1]);
    write_register(channel, ATA_REG_LBA2, lba_io[2]);

    if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;   
    if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;   
    if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
    write_register(channel, ATA_REG_COMMAND, cmd);

    /*
        After sending the command, we should poll, then we read/write a sector, 
        then we should poll, then we read/write a sector, until we read/write all sectors needed, 
        if an error has happened, the function will return a specific error code.

        Notice that after writing, we should execute the CACHE FLUSH command, 
        and we should poll after it, but without checking for errors.
    */
    if (direction == 0) { // PIO Read.
        for (int i = 0; i < numsects; i++) {
            if ((err = poll(channel, true)))
                return err;
            
            asm("pushw %es");
            asm("mov %%ax, %%es" : : "a"(data_seg));
            asm("rep insw" : : "c"(words), "d"(bus), "D"(data_ptr)); // Receive Data.
            asm("popw %es");
            data_ptr += (words*2);
        }
    } else { // PIO Write.
        for (int i = 0; i < numsects; i++) {
            poll(channel, false); // Polling.

            asm("pushw %ds");
            asm("mov %%ax, %%ds"::"a"(data_seg));
            asm("rep outsw"::"c"(words), "d"(bus), "S"(data_ptr)); // Send Data
            asm("popw %ds");
            data_ptr += (words*2);
        }

        uint8_t flush_cmd = lba_mode == 2 ? ATA_CMD_CACHE_FLUSH_EXT : ATA_CMD_CACHE_FLUSH;
        write_register(channel, ATA_REG_COMMAND, flush_cmd);
        poll(channel, false);
    }

    return 0;
}

static char read_sectors(struct pci_dev_driver_data *data, uint8_t drive_no, uint32_t lba, uint8_t numsects, uint16_t es, uint32_t edi) {

    klog_debug("read_sectors(drive_no=%d)", drive_no);

    // Check if the drive presents
    if (drive_no > 3 || !data->drives[drive_no].detected) {
        klog_debug("read_sectors() - a (detected=%d)", data->drives[drive_no].detected);
        return IDE_ERR_DRIVE_NOT_FOUND;
    }

    // Check if inputs are valid
    if (((lba + numsects) > data->drives[drive_no].size) && (data->drives[drive_no].type == IDE_ATA))
        return IDE_ERR_INVALID_ADDRESS;

    if (data->drives[drive_no].type != IDE_ATA) {
        klog_debug("read_sectors() - c");
        return IDE_ERR_DRIVE_NOT_FOUND;
    }

    // Read in PIO Mode through Polling & IRQs:
    return  ata_rw_operation(data, ATA_READ, drive_no, lba, numsects, es, edi);
}

static char write_sectors(struct pci_dev_driver_data *data, uint8_t drive_no, uint32_t lba, uint8_t numsects, uint16_t es, uint32_t edi) {

    // Check if the drive presents
    if (drive_no > 3 || !data->drives[drive_no].detected)
        return IDE_ERR_DRIVE_NOT_FOUND;

    // Check if inputs are valid
    if (((lba + numsects) > data->drives[drive_no].size) && (data->drives[drive_no].type == IDE_ATA))
        return IDE_ERR_INVALID_ADDRESS;

    // Read in PIO Mode through Polling & IRQs:
    if (data->drives[drive_no].type != IDE_ATA)
        return IDE_ERR_DRIVE_NOT_FOUND;

    return ata_rw_operation(data, ATA_WRITE, drive_no, lba, numsects, es, edi);
}

static uint8_t ide_get_specific_error(struct ide_channel *channel, uint8_t err) {

    uint8_t st = read_register(channel, ATA_REG_ERROR);
    if (st & ATA_ER_AMNF)  return IDE_ERR_ADDR_MARK_NOT_FOUND;
    if (st & ATA_ER_TK0NF) return IDE_ERR_NO_MEDIA;
    if (st & ATA_ER_ABRT)  return IDE_ERR_CMD_ABORTED;
    if (st & ATA_ER_MCR)   return IDE_ERR_NO_MEDIA;
    if (st & ATA_ER_IDNF)  return IDE_ERR_ID_MARK_NOT_FOUND;
    if (st & ATA_ER_MC)    return IDE_ERR_NO_MEDIA;
    if (st & ATA_ER_UNC)   return IDE_ERR_UNCORRECTABLE_DATA_ERROR;
    if (st & ATA_ER_BBK)   return IDE_ERR_BAD_SECTORS;

    return 0;
}

static int storage_dev_sector_size(struct storage_dev *dev) {
    (void)dev;
    return 512;
}

static int storage_dev_read(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer) {
    struct pci_dev_driver_data *driver_data = dev->pci_dev->driver_private_data;
    struct storage_dev_driver_data *storage_data = dev->priv_data;
    return read_sectors(driver_data, storage_data->drive_no, sector_low, sectors, 0, (uint32_t)buffer);
}

static int storage_dev_write(struct storage_dev *dev, uint32_t sector_low, uint32_t sector_hi, uint32_t sectors, char *buffer) {
    struct pci_dev_driver_data *driver_data = dev->pci_dev->driver_private_data;
    struct storage_dev_driver_data *storage_data = dev->priv_data;
    return write_sectors(driver_data, storage_data->drive_no, sector_low, sectors, 0, (uint32_t)buffer);
}

static struct storage_dev_ops ide_ops = {
    .sector_size = storage_dev_sector_size,
    .read = storage_dev_read,
    .write = storage_dev_write
};

static void register_storage_device(struct pci_dev_driver_data *data, int drive_no, pci_device_t *pci_device) {
    // prepare registration info
    char *name = kmalloc(80);
    sprintfn(name, 80, "IDE %s %s (drive_no #%d): %s",
        data->drives[drive_no].channel_no == 0 ? "primary" : "secondary",
        data->drives[drive_no].master_slave == 0 ? "master" : "slave",
        drive_no,
        data->drives[drive_no].model
    );

    struct storage_dev_driver_data *priv_data = kmalloc(sizeof(struct storage_dev_driver_data));
    memset(priv_data, 0, sizeof(struct storage_dev_driver_data));
    priv_data->drive_no = drive_no;

    struct storage_dev *storage_dev = kmalloc(sizeof(struct storage_dev));
    memset(storage_dev, 0, sizeof(struct storage_dev));
    storage_dev->name = name;
    storage_dev->pci_dev = pci_device;
    storage_dev->priv_data = priv_data;
    storage_dev->ops = &ide_ops;

    storage_mgr_register_device(storage_dev);
}

static bool probe_drive(struct ide_drive *drive, struct ide_channel *channel, char *buffer) {

    // select drive
    write_register(channel, ATA_REG_HDDEVSEL, 0xA0 | (drive->master_slave << 4));
    timer_pause_blocking(1); // Wait 1ms for drive select to work.

    // send ATA identify command
    write_register(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    timer_pause_blocking(1); // This function should be implemented in your OS. which waits for 1 ms. it is based on System Timer Device Driver.

    // polling
    if (!read_register(channel, ATA_REG_STATUS))
        return false; // If Status = 0, No Device.

    uint8_t type = IDE_ATA;
    uint8_t status;
    uint8_t err = 0;
    while(1) {
        status = read_register(channel, ATA_REG_STATUS);
        if (status & ATA_SR_ERR) { 
            err = 1; // if err, device is not ATA, maybe ATAPI
            break;
        } 

        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) 
            break; // everything is ok
    }
    if (err) {
        // could be an ATAPI device
        // uint8_t cl = read_register(channel, ATA_REG_LBA1);
        // uint8_t ch = read_register(channel, ATA_REG_LBA2);

        // if ((cl == 0x14 && ch ==0xEB) || (cl == 0x69 && ch ==0x96))
        //     type = IDE_ATAPI;
        // else
        //     continue; // unknown type (and always not be a device).

        // ide_write(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
        // timer_pause_blocking(1);
        klog_trace("Seems like an ATAPI device, not supported for now");
        return false; // we dont care to support ATAPI for now.
    }

    // read identification space of the device
    read_buffer_pio(channel, ATA_REG_DATA, (uint32_t *)buffer, 128);
    // klog_info("Identify results");
    // klog_hex16_info(buffer, 512, 0);

    drive->detected     = 1;
    drive->type         = type;
    drive->signature    = ((uint16_t *)(buffer + ATA_IDENT_DEVICETYPE   ))[0];
    drive->capabilities = ((uint16_t *)(buffer + ATA_IDENT_CAPABILITIES ))[0];
    drive->command_sets = ((uint32_t *)(buffer + ATA_IDENT_COMMANDSETS  ))[0];

    // Get Size:
    if (IS_BIT(drive->command_sets, 26)) {
        // Device uses 48-Bit Addressing:
        drive->size = ((uint32_t *)(buffer + ATA_IDENT_MAX_LBA_EXT))[0];
        // Note that Quafios is 32-Bit Operating System, So last 2 Words are ignored.
    } else {
        // Device uses CHS or 28-bit Addressing:
        drive->size = ((uint32_t *)(buffer + ATA_IDENT_MAX_LBA))[0];
    }

    // String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
    for(int i = 0; i < 40; i += 2) {
        drive->model[i]     = buffer[i + ATA_IDENT_MODEL + 1];
        drive->model[i + 1] = buffer[i + ATA_IDENT_MODEL];
    }
    drive->model[40] = 0; // terminate
    for (int i = 39; i > 0 && drive->model[i] == ' '; i--) {
        drive->model[i] = '\0'; // trim spaces from the right
    }

    return true;
}

// probing method, called from PCI driver, must return zero if successfully claimed device
static int probe(pci_device_t *pci_dev) {

    uint8_t pif = pci_dev->config.prog_if;
    klog_debug("IDE driver probing device %d:%d:%d, pif 0x%x (%8bb)", pci_dev->bus_no, pci_dev->device_no, pci_dev->func_no, pif, pif);

    struct pci_dev_driver_data *driver_data = kmalloc(sizeof(struct pci_dev_driver_data));
    memset(driver_data, 0, sizeof(struct pci_dev_driver_data));
    pci_dev->driver_private_data = driver_data;

    uint16_t bar0 = pci_dev->config.headers.h00.bar0 & 0xFFFFFFFC;
    uint16_t bar1 = pci_dev->config.headers.h00.bar1 & 0xFFFFFFFC;
    uint16_t bar2 = pci_dev->config.headers.h00.bar2 & 0xFFFFFFFC;
    uint16_t bar3 = pci_dev->config.headers.h00.bar3 & 0xFFFFFFFC;
    uint16_t bar4 = pci_dev->config.headers.h00.bar4 & 0xFFFFFFFC;

    // Detect I/O Ports which interface IDE Controller:
    driver_data->channels[PRIMARY_CHANNEL  ].base_port  = bar0 ? bar0 : 0x1F0;
    driver_data->channels[PRIMARY_CHANNEL  ].ctrl_port  = bar1 ? bar1 : 0x3F4;
    driver_data->channels[SECONDARY_CHANNEL].base_port  = bar2 ? bar2 : 0x170;
    driver_data->channels[SECONDARY_CHANNEL].ctrl_port  = bar3 ? bar3 : 0x374;
    driver_data->channels[PRIMARY_CHANNEL  ].bus_master_ide = bar4 + 0;
    driver_data->channels[SECONDARY_CHANNEL].bus_master_ide = bar4 + 8;   
    write_register(driver_data->channels + PRIMARY_CHANNEL,   ATA_REG_CONTROL, 2);
    write_register(driver_data->channels + SECONDARY_CHANNEL, ATA_REG_CONTROL, 2);

    char *page = allocate_physical_page();
    
    // enumerate devices
    for (int channel_no = 0; channel_no < 2; channel_no++) {
        for (int master_slave = 0; master_slave < 2; master_slave++) {
            int drive_no = channel_no * 2 + master_slave;
                    
            struct ide_channel *channel = &driver_data->channels[channel_no];
            struct ide_drive *drive = &driver_data->drives[drive_no];

            drive->detected = 0; // assuming no drive here.
            drive->channel_no   = channel_no;
            drive->master_slave = master_slave;

            if (!probe_drive(drive, channel, page))
                continue;
            
            klog_info("Detected %s %s %s drive: \"%s\"",
                drive->channel_no == 0 ? "primary" : "secondary",
                drive->master_slave == 0 ? "master" : "slave",
                drive->type == 0 ? "ATA" : "ATAPI",
                drive->model
            );

            // let's try to read a sector or two.
            // klog_info("Reading boot sector");
            // ata_rw_operation(ATA_READ, drive_no, 0, 1, 0, (uint32_t)buffer);
            // klog_hex16_info(buffer, 512, 0);
            // klog_info("Reading second sector");
            // ata_rw_operation(ATA_READ, drive_no, 1, 1, 0, (uint32_t)buffer);
            // klog_hex16_info(buffer, 512, 512);

            register_storage_device(driver_data, drive_no, pci_dev);
        }
    }

    free_physical_page(page);

    // let's print them
    klog_debug("IDE Drives:");
    for (int i = 0; i < 4; i++) {
        klog_debug("  %d: det=%d, chn=%d, slv=%d, sig=0x%x, cap=0x%x, cmd=0x%x \"%s\"",
            i,
            driver_data->drives[i].detected,
            driver_data->drives[i].channel_no,
            driver_data->drives[i].master_slave,
            driver_data->drives[i].signature,
            driver_data->drives[i].capabilities,
            driver_data->drives[i].command_sets,
            driver_data->drives[i].model
        );
    }

    return 0;
}

static struct pci_driver pci_driver = {
    .name = "IDE Controller Driver",
    .probe = probe
};

void ata_register_pci_driver() {
    register_pci_driver(1, 1, &pci_driver);
}
