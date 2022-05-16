#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "ports.h"
#include "string.h"
#include "screen.h"


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



void test_hdd() {
    
}
void init_hdd() {

}

void read_hdd() {

}

void write_hdd() {

}