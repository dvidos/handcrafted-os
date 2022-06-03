#ifndef _BITS_H
#define _BITS_H

#define SET_BIT(value, bitno)      ((value) | (1 << (bitno)))
#define CLEAR_BIT(value, bitno)    ((value) & ~(1 << (bitno)))
#define IS_BIT(value, bitno)       ((value) & (1 << (bitno)))


#define HIGH_BYTE(value)       (((value) >> 8) & 0xFF)
#define LOW_BYTE(value)        (((value) >> 0) & 0xFF)

#define HIGH_WORD(value)       (((value) >> 16) & 0xFFFF)
#define LOW_WORD(value)        (((value) >>  0) & 0xFFFF)
      
#define HIGH_DWORD(value)      (((value) >> 32) & 0xFFFFFFFF)
#define LOW_DWORD(value)       (((value) >>  0) & 0xFFFFFFFF)


#define FOURTH_BYTE(value)     (((value) >> 24) & 0xFF)
#define THIRD_BYTE(value)      (((value) >> 16) & 0xFF)
#define SECOND_BYTE(value)     (((value) >>  8) & 0xFF)
#define FIRST_BYTE(value)      (((value) >>  0) & 0xFF)
  
#define FOURTH_WORD(value)     (((value) >> 48) & 0xFFFF)
#define THIRD_WORD(value)      (((value) >> 32) & 0xFFFF)
#define SECOND_WORD(value)     (((value) >> 16) & 0xFFFF)
#define FIRST_WORD(value)      (((value) >>  0) & 0xFFFF)

#define SWAP_16(x)                  \
  ( (((x) >> 8) & 0xff)             \
  | (((x) & 0xff) << 8))

#define SWAP_32(x)                  \
    ( (((x) & 0xff000000u) >> 24)   \
    | (((x) & 0x00ff0000u) >> 8)    \
    | (((x) & 0x0000ff00u) << 8)    \
    | (((x) & 0x000000ffu) << 24))

#define SWAP_64(x)  \
    ( (((x) & 0xff00000000000000ull) >> 56)  \
    | (((x) & 0x00ff000000000000ull) >> 40)  \
    | (((x) & 0x0000ff0000000000ull) >> 24)  \
    | (((x) & 0x000000ff00000000ull) >> 8)   \
    | (((x) & 0x00000000ff000000ull) << 8)   \
    | (((x) & 0x0000000000ff0000ull) << 24)  \
    | (((x) & 0x000000000000ff00ull) << 40)  \
    | (((x) & 0x00000000000000ffull) << 56))


#endif
