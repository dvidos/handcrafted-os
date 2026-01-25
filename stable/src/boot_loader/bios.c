#include "types.h"
#include "funcs.h"

// for BIOS interrupts information
// http://www.ctyme.com/intr/int.htm




char num_buff[16+1];


void bios_print_char(unsigned char c) {
    // AH shall have 0x0e (print char function)
    // AL will have the byte to print
    __asm__(
        "mov $0x0e, %%ah \n\t"
        "mov %0, %%al \n\t"
        "int $0x10 \n\t"
        : // outputs
        : "g" (c) // inputs
        : // clobbers
    );
}

void bios_print_str(unsigned char *str) {
    while (*str) {
        // allow a single "\n" to cause "\r\n", as needed by bios
        if (*str == '\n')
            bios_print_char('\r');
        bios_print_char(*str);
        str++;
    }
}

void bios_print_int(int num) {
    itoa(num, num_buff, 10);
    bios_print_str(num_buff);
}

void bios_print_hex(int num) {
    itoa(num, num_buff, 16);
    bios_print_str(num_buff);
}

word bios_get_low_memory_in_kb() {
    // detecting low memory, INT 12
    word value = 0;
    word error = 0;

    asm(
        "clc            \n\t"  // clear carry
        "mov $0, %%dx \n\t"
        "int $0x12      \n\t"
        "jnc 1f    \n\t"  // carry will be set on error
        "mov $1, %%dx     \n\t"  // set error flag
        "1:       \n\t"
        : "=d" (error), 
          "=a" (value)  // outputs
        : // inputs
         // clobbered
    );

    return error ? 0 : value;
}

int bios_detect_memory_map_e820(void *buffer, int *times) {
    // call INT 0x15, EAX=0xE820 in a loop.
    // see https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15.2C_EAX_.3D_0xE820
    word error = 0;
    dword continuation = 0;

    // every pointed 24 bytes are filled as:
    // - 4 btyes base address low 32 bits
    // - 4 bytes base address high 32 bits
    // - 4 bytes length low 32 bit
    // - 4 bytes length high 32 bits
    // - 4 bytes type (and 4 bytes padding)
    (*times) = 0;
    while (true) {
        asm(
            "mov $0xE820, %%eax   \n\t"
            "mov %2, %%di   \n\t"   // pointer to data area (ES:DI actually)
            "mov %3, %%ebx   \n\t"   // continuation value form prev call
            "mov $24, %%ecx \n\t"  // buffer size
            "mov $0x534D4150, %%edx \n\t" // 'SMAP' magic value
            "int $0x15      \n\t"

            // check success
            "cmp $0x534D4150, %%eax \n\t"
            "jne 1f \n\t"  // jump (in not equal) to the next "1" forward label
            "jc 1f \n\t"  // if carry is set, it's an error
            "jmp 2f \n\t"
            "1: \n\t"
            "movw $1, %0 \n\t"   // error flag

            // if we are here, success
            "2:  \n\t"
            "mov %%ebx, %1 \n\t"  // continuation value

            // outputs
            : "=g" (error), 
            "=g" (continuation)
            // inputs
            : "g" (buffer),
            "g" (continuation)

            // clobbered
            : "%eax", "%ebx", "%ecx", "%edx", "%edi"
        );

        if (error)
            break;
        (*times) += 1;
        buffer += 24;
        if (continuation == 0)
            break;
    }

    return error ? ERROR : SUCCESS;
}

int bios_detect_memory_map_e801(word *kb_above_1mb, word *pg_64kb_above_16mb) {
    // see https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15.2C_AX_.3D_0xE801
    // CX/AX is contiguous kb above one MB
    // DX/BX is contiguoys 64KB pages above 16MB
    word error = 0;
    word ax = 0;
    word bx = 0;

    asm(
        "mov $0, %%cx \n\t"
        "mov $0, %%dx \n\t"
        "mov $0xE801, %%ax \n\t"
        "int $0x15      \n\t"

        // check success
        "jc 2f \n\t"  // if carry is set, it's an error
        "cmp $0x86, %%ah \n\t"
        "je 2f \n\r"  // ah=0x86 => unsupported function
        "cmp $0x80, %%ah \n\t"
        "je 2f \n\r"  // ah=0x80 => invalid command

        // if we are here, success
        "jcxz 1f  \r\n"        // if cx is zero, use the AX/BC set
        "mov %%cx, %%ax \r\n"  // otherwise copy CX/DX to AX/BX
        "mov %%dx, %%bx \r\n"
        "1:   \n\t"
        "mov %%ax, %1 \n\t" 
        "mov %%bx, %2 \n\t" 
        "jmp 3f \n\t"       // skip error handling

        // error handling
        "2: \n\t"
        "movw $1, %0 \n\t"   // error flag

        // exit
        "3: \n\t" 

        : "=g" (error), "=g" (ax), "=g" (bx)  // outputs
        :  // inputs
        : "%eax", "%ebx", "%ecx", "%edx"  // clobbered
    );

    *kb_above_1mb = ax;
    *pg_64kb_above_16mb = bx;
    return error ? ERROR : SUCCESS;
}


word bios_get_keystroke() {
    word output;
    asm(
        "mov $0x00, %%ah  \n\t"
        "int $0x16        \n\t"
        : "=a" (output) // ax -> output, al=ascii, ah=scan code
    );
}


int bios_get_drive_geometry(byte drive_no, byte *number_of_heads, byte *sectors_per_track) {
    word error = 0;
    byte dh;
    byte cl;

    asm(
        "mov $0x08, %%ah \n\t"  // get drive geometry
        "mov %3, %%dl    \n\t"  // drive number
        "int $0x13      \n\t"

        // check success
        "jc 2f \n\t"  // if carry is set, it's an error

        // if we are here, success
        "mov %%dh, %1 \n\t" 
        "mov %%cl, %2 \n\t" 
        "jmp 3f \n\t"       // skip error handling

        // error handling
        "2: \n\t"
        "movw $1, %0 \n\t"   // error flag

        // exit
        "3: \n\t" 

        : "=g" (error), "=g" (dh), "=g" (cl)  // outputs
        : "g" (drive_no)
        : "%eax", "%ebx", "%ecx", "%edx"  // clobbered
    );

    *number_of_heads = dh + 1;
    *sectors_per_track = cl & 0x3F;

    return error ? ERROR : SUCCESS;
}


// Cylinder, Head, Sector to Linear Block Address
word chs_to_lba(word cyl, byte head, byte sector, byte num_heads, byte sectors_per_track) {
    // note: sectors are one based, LBA is zero based
    return (((cyl * num_heads) + head) * sectors_per_track) + sector - 1;
}


// Linear Block Address to Cylinder, Head, Sector
void lba_to_chs(word lba, byte num_heads, byte sectors_per_track, word *cyl, byte *head, byte *sector) {
    word temp = lba / sectors_per_track;
    *sector = (lba % sectors_per_track) + 1;  // sectors are one based, not zero.
    *cyl = temp / num_heads;
    *head = temp % num_heads;
}


int bios_load_disk_sectors_lba(byte drive, word lba, byte sectors_count, void *buffer) {
    // sde https://wiki.osdev.org/ATA_in_x86_RealMode_(BIOS)
    // see https://stanislavs.org/helppc/int_13-2.html

    // byte error = 0;
    // byte status = 0;
    // byte sectors_read = 0;

    // asm(
    //     "mov $0,    %0     \n\t"  // no error by default

    //     "mov $0x42, %%ah   \n\t"  // read in CHS mode
    //     "mov $1,    %%al   \n\t"  // num of sectors to read (1-128)
    //     "mov %3,    %%ch   \n\t"  // track/cyl to read (0-1023)
    //     "mov %4,    %%cl   \n\t"  // sector number (1-17)
    //     "mov %5,    %%dh   \n\t"  // head number (0-15)
    //     "mov %6,    %%dl   \n\t"  // drive number (0x00, 0x01, ..., 0x80, 0x81, ...)
    //     "mov %7,    %%bx   \n\t"  // ES:BX points to buffer
    //     "int $0x13         \n\t"

    //     // check success
    //     "jnc 1f            \n\t"  // if no carry is set, it's not an error
    //     "mov $1,     %0    \n\t"  // otherwise we failed
    //     "1:   \n\t"

    //     // grab output
    //     "mov %%ah,   %1    \n\t"  // ah has status
    //     "mov %%al,   %2    \n\t"  // al has number of sectors read

    //     : "=g" (error), "=g" (status), "=g" (sectors_read)  // outputs
    //     : "g" (cyl & 0xFF), 
    //       "g" (((cyl >> 2) & 0xC0) | (sect & 0x3F)),
    //       "g" (head), 
    //       "g" (drive), 
    //       "g" (buffer) // inputs
    //     : "%eax", "%ebx", "%ecx", "%edx"  // clobbered
    // );

    // // for status see https://stanislavs.org/helppc/int_13-1.html
    // return error ? status : SUCCESS;

    return ERROR;
}

int bios_reset_disk(byte drive) {
    // see https://stanislavs.org/helppc/int_13-0.html

    byte error;
    byte status;

    asm(
        "mov $0,    %0     \n\t"  // no error by default

        "mov $0x00, %%ah   \n\t"  // 00 = reset disk system
        "mov %2,    %%dl   \n\t"  // drive number (0x00, 0x01, ..., 0x80, 0x81, ...)
        "int $0x13         \n\t"

        // check success
        "jnc 1f            \n\t"  // if no carry is set, it's not an error
        "mov $1,     %0    \n\t"  // otherwise we failed
        "1:   \n\t"

        // grab output
        "mov %%ah,   %1    \n\t"  // ah has status

        : "=g" (error), "=g" (status)  // outputs
        : "g" (drive)                  // inputs
        : "%eax", "%edx"               // clobbered
    );

    // for status see https://stanislavs.org/helppc/int_13-1.html
    return error ? status : SUCCESS;
}

int bios_load_disk_sector_chs(byte drive, word cyl, byte head, byte sect, void *buffer) {
    // limitation: total sector count must be up to 127, and cannot cross a cylinder boundary.
    // workaround is to load one sector at a time, when working on CHS mode
    // buffer and bytes to read should not cross a 64K boundary

    // byte error = 0;
    // byte status = 0;
    // byte sectors_read = 0;

    // asm(
    //     "movb $0,   %0     \n\t"  // no error by default
        
    //     "movb $0x02, %%ah   \n\t"  // read in CHS mode
    //     "movb $1,    %%al   \n\t"  // num of sectors to read (1-128)
    //     "mov %3,    %%ch   \n\t"  // track/cyl to read (0-1023)
    //     "mov %4,    %%cl   \n\t"  // sector number (1-17)
    //     "mov %5,    %%dh   \n\t"  // head number (0-15)
    //     "mov %6,    %%dl   \n\t"  // drive number (0x00, 0x01, ..., 0x80, 0x81, ...)
    //     "mov %7,    %%bx   \n\t"  // ES:BX points to buffer
    //     "int $0x13         \n\t"

    //     // check success
    //     "jnc 1f            \n\t"  // if no carry is set, it's not an error
    //     "mov $1,     %0    \n\t"  // otherwise we failed
    //     "1:   \n\t"

    //     // grab output
    //     "mov %%ah,   %1    \n\t"  // ah has status
    //     "mov %%al,   %2    \n\t"  // al has number of sectors read

    //     : "=g" (error), "=g" (status), "=g" (sectors_read)  // outputs
    //     : "g" (cyl & 0xFF), 
    //       "g" (((cyl >> 2) & 0xC0) | (sect & 0x3F)),
    //       "g" (head), 
    //       "g" (drive), 
    //       "g" (buffer) // inputs
    //     : "%eax", "%ebx", "%ecx", "%edx"  // clobbered
    // );

    // // for status see https://stanislavs.org/helppc/int_13-1.html
    // return error ? status : SUCCESS;

    return ERROR;
}


