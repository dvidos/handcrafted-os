#ifndef _BITS_H
#define _BITS_H

#define SET_BIT(value, bitno)      ((value) | (1 << (bitno)))
#define CLEAR_BIT(value, bitno)    ((value) & ~(1 << (bitno)))
#define IS_BIT(value, bitno)       ((value) & (1 << (bitno)))


#endif
