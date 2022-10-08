#ifndef _ERRORS_H
#define _ERRORS_H


// typical unix / linux philosophy, given that it's easy for a function to return an int.
// zero:      success
// negative:  error
// positive:  some value

#define SUCCESS                   NO_ERROR
#define NO_ERROR                  0

#define ERR_NOT_FOUND             -1   // usually returned from strpos() etc
#define ERR_NO_MORE_CONTENT       -2   // for functions that read requential content
#define ERR_PARTIAL_CONTENT_ONLY  -3   // reading dirs etc, means loading next sector is needed
#define ERR_NOT_SUPPORTED         -4
#define ERR_NOT_IMPLEMENTED       -5
#define ERR_BAD_ARGUMENT          -6
#define ERR_BAD_VALUE             -7

#define ERR_NO_DEVICE            -10
#define ERR_NO_PARTITION         -11
#define ERR_NO_DRIVER_FOUND      -12
#define ERR_NOT_A_DIRECTORY      -13
#define ERR_NOT_A_FILE           -14
#define ERR_NO_SPACE_LEFT        -15
#define ERR_NO_FS_MOUNTED        -16
#define ERR_NO_RUNNING_PROCESS   -17

#define ERR_READING_FILE         -20
#define ERR_WRITING_FILE         -21
#define ERR_HANDLES_EXHAUSTED    -22


#endif