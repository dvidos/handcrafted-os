#ifndef _MACROS_H
#define _MACROS_H



#define max(a, b)                   ((a) > (b) ? (a) : (b))
#define min(a, b)                   ((a) < (b) ? (a) : (b))

#define clamp(value, a, b)          ((value) < (a) ? (a) : ((value) > (b) ? (b) : (value)))

#define round_up(value, unit)       ((((value) + (unit) - 1)/(unit))*(unit))
#define round_down(value, unit)     ((((value)             )/(unit))*(unit))






#endif // _MACROS_H