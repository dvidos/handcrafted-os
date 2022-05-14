#ifndef _PORTS_H
#define _PORTS_H

#include <stdint.h>


void outb(uint16_t port, uint8_t data);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t data);
uint16_t inw(uint16_t port);


#endif

