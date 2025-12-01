#pragma once

// returned by int returning functions.
// 0=success, negative=errors, positive=values

#define OK                        0
#define ERR_NOT_IMPLEMENTED      -1
#define ERR_NOT_SUPPORTED        -2
#define ERR_OUT_OF_BOUNDS        -3
#define ERR_NOT_PERMITTED        -4
#define ERR_NOT_RECOGNIZED       -5
#define ERR_RESOURCES_EXHAUSTED  -6 
#define ERR_WRONG_TYPE           -7  // e.g. open_dir() on a file
#define ERR_INVALID_ARGUMENT     -8
#define ERR_NOT_FOUND            -9
#define ERR_END_OF_FILE         -10
#define ERR_ALREADY_EXISTS      -11  // e.g. resource already exists

