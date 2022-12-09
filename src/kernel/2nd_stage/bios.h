#ifndef _BIOS_H
#define _BIOS_H


void bios_print_char(unsigned char c);
void bios_print_str(unsigned char *str);
void bios_print_int(int num);
void bios_print_hex(int num);
word bios_get_low_memory_in_kb();
int bios_detect_memory_map_e820(void *buffer, int *times);


word bios_get_keystroke();



#endif
