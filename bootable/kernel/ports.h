#ifndef _PORTS_H
#define _PORTS_H


void port_byte_out(uint16_t port, uint8_t data);
uint8_t port_byte_in(uint16_t port);
void port_word_out(uint16_t port, uint16_t data);
uint16_t port_word_in(uint16_t port);


#endif

