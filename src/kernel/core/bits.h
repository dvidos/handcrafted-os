#ifndef _BITS_H
#define _BITS_H

#define SET_BIT(value, bitno)      ((value) | (1 << (bitno)))
#define CLEAR_BIT(value, bitno)    ((value) & ~(1 << (bitno)))
#define IS_BIT(value, bitno)       ((value) & (1 << (bitno)))


#define HIGH_8(value)          (((value) >> 8) & 0xFF)
#define LOW_8(value)           (((value) >> 0) & 0xFF)
      
#define HIGH_16(value)         (((value) >> 16) & 0xFFFF)
#define LOW_16(value)          (((value) >>  0) & 0xFFFF)
      
#define HIGH_32(value)         (((value) >> 32) & 0xFFFFFFFF)
#define LOW_32(value)          (((value) >>  0) & 0xFFFFFFFF)


#define FOURTH_BYTE(value)     (((value) >> 24) & 0xFF)
#define THIRD_BYTE(value)      (((value) >> 16) & 0xFF)
#define SECOND_BYTE(value)     (((value) >>  8) & 0xFF)
#define FIRST_BYTE(value)      (((value) >>  0) & 0xFF)
  
#define FOURTH_WORD(value)     (((value) >> 48) & 0xFFFF)
#define THIRD_WORD(value)      (((value) >> 32) & 0xFFFF)
#define SECOND_WORD(value)     (((value) >> 16) & 0xFFFF)
#define FIRST_WORD(value)      (((value) >>  0) & 0xFFFF)


#endif
