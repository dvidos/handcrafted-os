#ifndef _ERRORS_H
#define _ERRORS_H


// typical unix / linux philosophy, given that it's easy for a function to return an int.
// zero:       success
// negative:   error
// posigtive:  some value

#define NO_ERROR 0
#define ERR_NOT_FOUND        -1   // usually returned from strpos() etc
#define ERR_NO_DEVICE        -2
#define ERR_NO_PARTITION     -3
#define ERR_NO_DRIVER_FOUND  -4
#define ERR_NOT_SUPPORTED    -5
#define ERR_NOT_IMPLEMENTED  -6






#endif