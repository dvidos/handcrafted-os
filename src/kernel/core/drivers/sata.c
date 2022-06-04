#include <stdint.h>
#include <klib/string.h>
#include <memory/physmem.h>
#include <drivers/pci.h>
#include <klog.h>
#include <bits.h>
#include <drivers/screen.h>

// based on https://wiki.osdev.org/AHCI
// and https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1-3-1.pdf

/*
 * An AHCI device (known also as HBA - Host Bus Adapter) 
 *     is a device for moving data between memory and SATA devices.
 * It can support up to 32 ports / devices. 
 * Is can support both PIO and DMA protocols
 * It may support command list per port to reduce overhead.
 * It may optionally support 64 bit addressing
 * AHCI describes the memory structure to communicate with the SATA devices
 */

void *allocated_physical_memory_per_port[32];

// FIS - Frame Information Structure
typedef enum {
    FIS_TYPE_REG_H2D    = 0x27,  // Register FIS - host to device
    FIS_TYPE_REG_D2H    = 0x34,  // Register FIS - device to host
    FIS_TYPE_DMA_ACT    = 0x39,  // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP  = 0x41,  // DMA setup FIS - bidirectional
    FIS_TYPE_DATA       = 0x46,  // Data FIS - bidirectional
    FIS_TYPE_BIST       = 0x58,  // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP  = 0x5F,  // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS   = 0xA1,  // Set device bits FIS - device to host
} FIS_TYPE;

// A host to device register FIS is used by the host to send command or control to a device. 
typedef struct fis_reg_host_to_device_struct {
    // DWORD 0
    uint8_t  fis_type;    // FIS_TYPE_REG_H2D
    uint8_t  pmport:4;    // Port multiplier
    uint8_t  reserved0:3;        // Reserved
    uint8_t  c:1;        // 1: Command, 0: Control
    uint8_t  command;    // Command register
    uint8_t  featurel;    // Feature register, 7:0
    // DWORD 1
    uint8_t  lba0;        // LBA low register, 7:0
    uint8_t  lba1;        // LBA mid register, 15:8
    uint8_t  lba2;        // LBA high register, 23:16
    uint8_t  device;        // Device register
    // DWORD 2
    uint8_t  lba3;        // LBA register, 31:24
    uint8_t  lba4;        // LBA register, 39:32
    uint8_t  lba5;        // LBA register, 47:40
    uint8_t  featureh;    // Feature register, 15:8
    // DWORD 3
    uint8_t  countl;        // Count register, 7:0
    uint8_t  counth;        // Count register, 15:8
    uint8_t  icc;        // Isochronous command completion
    uint8_t  control;    // Control register
    // DWORD 4
    uint8_t  reserved1[4];    // Reserved
} FIS_REG_HOST_TO_DEVICE;

// A device to host register FIS is used by the device 
// to notify the host that some ATA register has changed. 
// It contains the updated task files such as status, error and other registers.
typedef struct reg_device_to_host_struct {
    // DWORD 0
    uint8_t  fis_type;    // FIS_TYPE_REG_D2H
    uint8_t  pmport:4;    // Port multiplier
    uint8_t  reserved0:2;      // Reserved
    uint8_t  i:1;         // Interrupt bit
    uint8_t  reserved1:1;      // Reserved
    uint8_t  status;      // Status register
    uint8_t  error;       // Error register
    // DWORD 1
    uint8_t  lba0;        // LBA low register, 7:0
    uint8_t  lba1;        // LBA mid register, 15:8
    uint8_t  lba2;        // LBA high register, 23:16
    uint8_t  device;      // Device register
    // DWORD 2
    uint8_t  lba3;        // LBA register, 31:24
    uint8_t  lba4;        // LBA register, 39:32
    uint8_t  lba5;        // LBA register, 47:40
    uint8_t  reserved2;        // Reserved
    // DWORD 3
    uint8_t  countl;      // Count register, 7:0
    uint8_t  counth;      // Count register, 15:8
    uint8_t  reserved3[2];     // Reserved
    // DWORD 4
    uint8_t  reserved4[4];     // Reserved
} FIS_REG_DEVICE_TO_HOST;


// This FIS is used by the host or device to send data payload. The data size can be varied.
typedef struct fis_data_struct {
    // DWORD 0
    uint8_t  fis_type;    // FIS_TYPE_DATA
    uint8_t  pmport:4;    // Port multiplier
    uint8_t  reserved0:4;        // Reserved
    uint8_t  reserved1[2];    // Reserved
    // DWORD 1 ~ N
    uint32_t data[1];    // Payload
} FIS_DATA;

typedef struct fis_pio_setup_struct {
    // DWORD 0
    uint8_t  fis_type;    // FIS_TYPE_PIO_SETUP
    uint8_t  pmport:4;    // Port multiplier
    uint8_t  reserved0:1;        // Reserved
    uint8_t  d:1;        // Data transfer direction, 1 - device to host
    uint8_t  i:1;        // Interrupt bit
    uint8_t  reserved1:1;
    uint8_t  status;        // Status register
    uint8_t  error;        // Error register
    // DWORD 1
    uint8_t  lba0;        // LBA low register, 7:0
    uint8_t  lba1;        // LBA mid register, 15:8
    uint8_t  lba2;        // LBA high register, 23:16
    uint8_t  device;        // Device register
    // DWORD 2
    uint8_t  lba3;        // LBA register, 31:24
    uint8_t  lba4;        // LBA register, 39:32
    uint8_t  lba5;        // LBA register, 47:40
    uint8_t  reserved2;        // Reserved
    // DWORD 3
    uint8_t  countl;        // Count register, 7:0
    uint8_t  counth;        // Count register, 15:8
    uint8_t  reserved3;        // Reserved
    uint8_t  e_status;    // New value of status register
    // DWORD 4
    uint16_t tc;        // Transfer count
    uint8_t  reserved4[2];    // Reserved
} FIS_PIO_SETUP;

// dma setup, device to host
typedef struct fis_dma_setup_struct {
    // DWORD 0
    uint8_t  fis_type;    // FIS_TYPE_DMA_SETUP
    uint8_t  pmport:4;    // Port multiplier
    uint8_t  reserved0:1;        // Reserved
    uint8_t  d:1;        // Data transfer direction, 1 - device to host
    uint8_t  i:1;        // Interrupt bit
    uint8_t  a:1;            // Auto-activate. Specifies if DMA Activate FIS is needed
    uint8_t  reserveded[2];       // Reserved
    //DWORD 1&2
    uint64_t DMAbufferID;    // DMA Buffer Identifier. Used to Identify DMA buffer in host memory.
                                 // SATA Spec says host specific and not in Spec. Trying AHCI spec might work.
    //DWORD 3
    uint32_t reservedd;           //More reserved
    //DWORD 4
    uint32_t DMAbufOffset;   //Byte offset into buffer. First 2 bits must be 0
    //DWORD 5
    uint32_t TransferCount;  //Number of bytes to transfer. Bit 0 must be 0
    //DWORD 6
    uint32_t resvd;          //Reserved
} FIS_DMA_SETUP;

// up to 32 ports per sata controller
typedef volatile struct hba_port_struct {
    uint32_t clb;		// 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;		// 0x04, command list base address upper 32 bits
    uint32_t fb;		// 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;		// 0x0C, FIS base address upper 32 bits
    uint32_t is;		// 0x10, interrupt status
    uint32_t ie;		// 0x14, interrupt enable
    uint32_t cmd;		// 0x18, command and status
    uint32_t rsv0;		// 0x1C, Reserved
    uint32_t tfd;		// 0x20, task file data
    uint32_t sig;		// 0x24, signature
    uint32_t ssts;		// 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;		// 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;		// 0x30, SATA error (SCR1:SError)
    uint32_t sact;		// 0x34, SATA active (SCR3:SActive)
    uint32_t ci;		// 0x38, command issue
    uint32_t sntf;		// 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;		// 0x40, FIS-based switch control
    uint32_t rsv1[11];	// 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4];	// 0x70 ~ 0x7F, vendor specific
} HBA_PORT;


// Host Bus Adapter (AHCI controller) memory registers
// that memory should be marked uncacheable
typedef volatile struct hba_mem_struct {
    // 0x00 - 0x2B, Generic Host Control
    uint32_t cap;		// 0x00, Host capability
    uint32_t ghc;		// 0x04, Global host control
    uint32_t is;		// 0x08, Interrupt status
    uint32_t pi;		// 0x0C, Port implemented
    uint32_t vs;		// 0x10, Version
    uint32_t ccc_ctl;	// 0x14, Command completion coalescing control
    uint32_t ccc_pts;	// 0x18, Command completion coalescing ports
    uint32_t em_loc;		// 0x1C, Enclosure management location
    uint32_t em_ctl;		// 0x20, Enclosure management control
    uint32_t cap2;		// 0x24, Host capabilities extended
    uint32_t bohc;		// 0x28, BIOS/OS handoff control and status
 
    // 0x2C - 0x9F, Reserved
    uint8_t  rsv[0xA0-0x2C];
 
    // 0xA0 - 0xFF, Vendor specific registers
    uint8_t  vendor[0x100-0xA0];
 
    // 0x100 - 0x10FF, Port control registers
    HBA_PORT	ports[32];	// 1 ~ 32
} HBA_MEM;
 
// FIS is a packet of information transferred between host and drive
// pointed by fb+fbu of the hba port structure
typedef volatile struct hba_fis_struct {
    // 0x00
    FIS_DMA_SETUP	dsfis;		// DMA Setup FIS
    uint8_t         pad0[4];
    // 0x20
    FIS_PIO_SETUP	psfis;		// PIO Setup FIS
    uint8_t         pad1[12];
    // 0x40
    FIS_REG_DEVICE_TO_HOST   rfis;		// Register – Device to Host FIS
    uint8_t         pad2[4];
    // 0x58
    uint16_t	    sdbfis;		// Set Device Bit FIS
    // 0x60
    uint8_t         ufis[64];
    // 0xA0
    uint8_t   	rsv[0x100-0xA0];
} HBA_FIS;

typedef struct hba_cmd_header_struct {
    // DW0
    uint8_t  cfl:5;		// Command FIS length in DWORDS, 2 ~ 16
    uint8_t  a:1;		// ATAPI
    uint8_t  w:1;		// Write, 1: H2D, 0: D2H
    uint8_t  p:1;		// Prefetchable
     uint8_t  r:1;		// Reset
    uint8_t  b:1;		// BIST
    uint8_t  c:1;		// Clear busy upon R_OK
    uint8_t  rsv0:1;		// Reserved
    uint8_t  pmp:4;		// Port multiplier port
     uint16_t prdtl;		// Physical region descriptor table length in entries
     // DW1
    volatile uint32_t prdbc;		// Physical region descriptor byte count transferred
     // DW2, 3
    uint32_t ctba;		// Command table descriptor base address
    uint32_t ctbau;		// Command table descriptor base address upper 32 bits
    // DW4 - 7
    uint32_t rsv1[4];	// Reserved
} HBA_CMD_HEADER;

// physical region descriptor table, for commands
typedef struct tagHBA_PRDT_ENTRY
{
    uint32_t dba;		// Data base address
    uint32_t dbau;		// Data base address upper 32 bits
    uint32_t rsv0;		// Reserved
    // DW3
    uint32_t dbc:22;		// Byte count, 4M max
    uint32_t rsv1:9;		// Reserved
    uint32_t i:1;		// Interrupt on completion
} HBA_PRDT_ENTRY;

// command table, pointed by cmd_header ctba/ctbau
typedef struct tagHBA_CMD_TBL
{
    // 0x00
    uint8_t  cfis[64];	// Command FIS
    // 0x40
    uint8_t  acmd[16];	// ATAPI command, 12 or 16 bytes
    // 0x50
    uint8_t  rsv[48];	// Reserved
    // 0x80
    HBA_PRDT_ENTRY	prdt_entry[4];	// Physical region descriptor table entries, 0 ~ 65535
} HBA_CMD_TBL;

// signature in a port 
#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	    0x96690101	// Port multiplier
 
// bits in sata status register
#define HBA_PORT_IPM_ACTIVE  1
#define HBA_PORT_DET_PRESENT 3

// port commands
#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000
 





static void send_command(HBA_PORT *port) {
    /* 
        To send a command, the host constructs a command header, 
        and set the according bit in the Port Command Issue register 
        (HBA_PORT.ci). The AHCI controller will automatically send the 
        command to the device and wait for response. If there are some 
        errors, error bits in the Port Interrupt register (HBA_PORT.is) 
        will be set and additional information can be retrieved from 
        the Port Task File register (HBA_PORT.tfd), SStatus register 
        (HBA_PORT.ssts) and SError register (HBA_PORT.serr). If it succeeds, 
        the Command Issue register bit will be cleared and the received data 
        payload, if any, will be copied from the device to the host memory 
        by the AHCI controller.
    */
}

void stop_command_engine(HBA_PORT *port) {
    // Clear ST (bit0)
    port->cmd &= ~HBA_PxCMD_ST;
 
    // Clear FRE (bit4)
    port->cmd &= ~HBA_PxCMD_FRE;
 
    // Wait until FR (bit14), CR (bit15) are cleared
    while(1) {
        if (port->cmd & HBA_PxCMD_FR)
            continue;
        if (port->cmd & HBA_PxCMD_CR)
            continue;
        break;
    }
}

void start_command_engine(HBA_PORT *port) {
    // Wait until CR (bit15) is cleared
    while (port->cmd & HBA_PxCMD_CR);
 
    // Set FRE (bit4) and ST (bit0)
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST; 
}

void rebase_port(HBA_PORT *port) {
    /* Each port can attach a single SATA device. 
     * Host sends commands to the device using Command List 
     * and device delivers information to the host using Received FIS structure. 
     * They are located at HBA_PORT.clb/clbu, and HBA_PORT.fb/fbu. 
     * 
     * The most important part of AHCI initialization is 
     * to set correctly these two pointers and the data structures they point to.
     * 
     * clb/clbu: HBA_CMD_HEADER list, 32 commands of 32 bytes each, 1k
     * fb/fbu  : HBA_FIS struct, 256 bytes
     * 
     * Each command header points to a command table that has a number of PRDTs 
     * (Physical Region Descriptor Tables) for data transfer
     * prdtl: Number of PRDTs
     * 
     * In our case, we'll allocate per page:
     * - 32 CMD_HEADER entries of 32 bytes => 1k
     * - 1 HBA_FIS entry of 256 bytes => 256 bytes
     * - 32 CMD_TABLEs having 8 PRDTs each (32 * 256 bytes) => 8192
     * A total of: 9472 bytes, rounded up to 3 pages of 4K => 12288
     */
    int port_memory_size = 12288;
    char *port_memory_addr = allocate_consecutive_physical_pages(port_memory_size);
    if (port_memory_addr == NULL)
        panic("Cannot allocate 12K for SATA drive");
    memset(port_memory_addr, 0, port_memory_size);

    // for some reason, these are not exactly the sizes, they are somewhat smaller...
    if (sizeof(HBA_CMD_HEADER) > 32)
        panic("Was expecting CMD_HEADER to be 32 bytes long");
    if (sizeof(HBA_FIS) > 256)
        panic("Was expecting FIS to be 256 bytes long");
    if (sizeof(HBA_CMD_TBL) > 256)
        panic("Was expecting CMD_TBL to be 256 bytes long");

    // if we don't stop the engine, 
    // a half-delivered FIS may lead to bad data
    stop_command_engine(port);	
 
    // point to Command List (1K)
    port->clb = (uint32_t)port_memory_addr + 0;
    port->clbu = 0;
 
    // point to Receiving FIS (256 bytes)
    port->fb = (uint32_t)port_memory_addr + 1024;
    port->fbu = 0;
 
    // point to Command Table (32 entries of 256 bytes)
    HBA_CMD_HEADER *cmd_hdr = (HBA_CMD_HEADER *)(port->clb);
    for (int cmd_no = 0; cmd_no < 32; cmd_no++) {
        cmd_hdr[cmd_no].prdtl = 8; // how many PRDTs per command table
        cmd_hdr[cmd_no].ctba = (uint32_t)port_memory_addr + 1024 + 256 + (cmd_no * 256);
        cmd_hdr[cmd_no].ctbau = 0;
    }
    
    start_command_engine(port);
}

// probing method, must return zero if successfully claimed device
int probe(pci_device_t *dev) {
    uint32_t base_mem_register = dev->config.headers.h00.bar5;
    klog_debug("base mem reg for sata is %x", base_mem_register);
    klog_debug("interrupt pin %d, interrupt line %d", dev->config.headers.h00.interrupt_pin, dev->config.headers.h00.interrupt_line);
    if (base_mem_register == 0)
        return -1;

    HBA_MEM *memory = (HBA_MEM *)base_mem_register;
    klog_debug("AHCI memory area");
    klog_debug("  Host capability             : 0x%x", memory->cap);
    klog_debug("  Global host control         : 0x%x", memory->ghc);
    klog_debug("  Interrupt status            : 0x%x", memory->is);
    klog_debug("  Port Implimented            : 0x%x (%bb)", memory->pi, memory->pi);
    klog_debug("  Version                     : 0x%x", memory->vs);
    klog_debug("  Cmd compl coalescing control: 0x%x", memory->ccc_ctl);
    klog_debug("  Cmd compl coalescing ports  : 0x%x", memory->ccc_pts);
    klog_debug("  Encl mgm location           : 0x%x", memory->em_loc);
    klog_debug("  Encl mgm control            : 0x%x", memory->em_ctl);
    klog_debug("  Host capabilities ext       : 0x%x", memory->cap2);
    klog_debug("  BIOS handoff ctrl/status    : 0x%x", memory->bohc);

    for (int port_no = 0; port_no < 32; port_no++) {
        if (!IS_BIT(memory->pi, port_no))
            continue;
        
        HBA_PORT *port = &memory->ports[port_no];
        klog_debug("Port %d, status 0x%x (%bb), signature 0x%x", port_no, port->ssts, port->ssts, port->sig);
        uint8_t interface_power_mgm = (port->ssts >> 8) & 0xF;
        uint8_t device_detection = (port->ssts & 0xF);

        if (interface_power_mgm != HBA_PORT_IPM_ACTIVE) {
            klog_debug("Device not active");
            continue;
        }
        if (device_detection != HBA_PORT_DET_PRESENT) {
            klog_debug("Device not powered");
            continue;
        }
        if (port->sig != SATA_SIG_ATA) {
            klog_debug("Device non ATA");
            continue;
        }
        // device is ata, powered, present
        // set memory areas pointers for data transfer
        rebase_port(port);




        klog_debug("Device is GOOD!");
    }
    return 0;
}

struct pci_driver sata_driver;
void sata_register_driver() {
    memset(allocated_physical_memory_per_port, 0, sizeof(allocated_physical_memory_per_port));

    sata_driver.name = "SATA Driver";
    sata_driver.probe = probe;
    register_pci_driver(1, 6, &sata_driver);
}