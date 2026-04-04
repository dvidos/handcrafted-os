#ifndef _FLOAT_H
#define _FLOAT_H

// A minimal float.h
// These macros describe characteristics of floating-point types.
// The values are system-dependent. These are typical IEEE 754 values.

#define FLT_RADIX       2               // Radix of exponent representation
#define FLT_EPSILON     1.19209290E-7F  // Smallest positive float x such that 1.0 + x != 1.0
#define FLT_MAX         3.40282347E+38F // Maximum representable finite float

#define DBL_EPSILON     2.2204460492503131E-16 // Smallest positive double x such that 1.0 + x != 1.0
#define DBL_MAX         1.7976931348623157E+308 // Maximum representable finite double

#define LDBL_EPSILON    2.2204460492503131E-16L // Smallest positive long double x such that 1.0 + x != 1.0
#define LDBL_MAX        1.7976931348623157E+308L // Maximum representable finite long double

#endif // _FLOAT_H