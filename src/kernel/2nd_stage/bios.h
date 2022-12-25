#ifndef _BIOS_H
#define _BIOS_H


void bios_print_char(unsigned char c);
void bios_print_str(unsigned char *str);
void bios_print_int(int num);
void bios_print_hex(int num);
word bios_get_low_memory_in_kb();
int bios_detect_memory_map_e820(void *buffer, int *times);
int bios_detect_memory_map_e801(word *kb_above_1mb, word *pg_64kb_above_16mb);

word bios_get_keystroke();

int bios_get_drive_geometry(byte drive_no, byte *number_of_heads, byte *sectors_per_track);
word chs_to_lba(word cyl, byte head, byte sector, byte num_heads, byte sectors_per_track);
void lba_to_chs(word lba, byte num_heads, byte sectors_per_track, word *cyl, byte *head, byte *sector);
int bios_load_disk_sectors_lba(byte drive, word lba, byte sectors_count, void *buffer);
int bios_load_disk_sector_chs(byte drive, word cyl, byte head, byte sect, void *buffer);



#endif
