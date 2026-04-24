#ifndef _MATH_H
#define _MATH_H

// --- Constants ---
#define M_PI 3.14159265358979323846 // Pi

// --- Function Prototypes (minimal set based on usage) ---

// Trigonometric functions
double sin(double x);
double cos(double x);
double tan(double x);

// Exponential and logarithmic functions
double exp(double x);
double log(double x); // Natural logarithm

// Power functions
double sqrt(double x);

// Absolute value
double fabs(double x);

// Ceiling, floor, truncation, and rounding functions
double ceil(double x);
double floor(double x);
double fmod(double x, double y);
double round(double x);

// Floating-point manipulation
double fmax(double x, double y);
double fmin(double x, double y);

#endif // _MATH_H