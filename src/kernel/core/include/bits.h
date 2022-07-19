#ifndef _BITS_H
#define _BITS_H

#define MASK_ON(bitno)            ((1 << (bitno)))
#define MASK_OFF(bitno)           (~(MASK_ON(bitno)))

#define IS_BIT(value, bitno)       ((value) & (1 << (bitno)))
#define SET_BIT(value, bitno)      ((value) | MASK_ON(bitno))
#define CLEAR_BIT(value, bitno)    ((value) & MASK_OFF(bitno))

#define HIGH_BYTE(value)       (uint8_t)(((value) >> 8) & 0xFF)
#define LOW_BYTE(value)        (uint8_t)(((value) >> 0) & 0xFF)

#define HIGH_WORD(value)       (uint16_t)(((value) >> 16) & 0xFFFF)
#define LOW_WORD(value)        (uint16_t)(((value) >>  0) & 0xFFFF)
      
#define HIGH_DWORD(value)      (uint32_t)(((value) >> 32) & 0xFFFFFFFF)
#define LOW_DWORD(value)       (uint32_t)(((value) >>  0) & 0xFFFFFFFF)


#define FOURTH_BYTE(value)     (uint8_t)(((value) >> 24) & 0xFF)
#define THIRD_BYTE(value)      (uint8_t)(((value) >> 16) & 0xFF)
#define SECOND_BYTE(value)     (uint8_t)(((value) >>  8) & 0xFF)
#define FIRST_BYTE(value)      (uint8_t)(((value) >>  0) & 0xFF)
  
#define FOURTH_WORD(value)     (uint16_t)(((value) >> 48) & 0xFFFF)
#define THIRD_WORD(value)      (uint16_t)(((value) >> 32) & 0xFFFF)
#define SECOND_WORD(value)     (uint16_t)(((value) >> 16) & 0xFFFF)
#define FIRST_WORD(value)      (uint16_t)(((value) >>  0) & 0xFFFF)


// e.g. MASK_OF(4) --> 0xF
#define MASK64_OF(bits_count)     (0xFFFFFFFFFFFFFFFFu >> (64 - (bits_count)))
#define MASK32_OF(bits_count)     (        0xFFFFFFFFu >> (32 - (bits_count)))

// for example BIT_RANGE(address, 12, 5), bit numbers are inclusive and zero-based
#define BIT_RANGE(value, hi_bit_no, lo_bit_no)     (((value)>>(lo_bit_no)) & MASK64_OF((hi_bit_no)-(lo_bit_no)+1))

// for example SET_BIT_RANGE(buffer, day, 12, 10). Bit nums are inclusive, zero based
#define SET_BIT_RANGE(value, hi_bit_no, lo_bit_no)   \
    (((value) & MASK64_OF((hi_bit_no)-(lo_bit_no)+1)) << (lo_bit_no))

// useful in memory paging
#define ROUND_UP_4K(number)      (((number) + 0xFFF) & ~0xFFF)
#define ROUND_DOWN_4K(number)    (((number)        ) & ~0xFFF)



// linux has smarter approach, processor specific mactos
// FROM/TO LITTLE/LARGE ENDIAN ()

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
