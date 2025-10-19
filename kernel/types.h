#ifndef TYPES_H
#define TYPES_H

// Basic type definitions for freestanding environment
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint32_t           size_t;
typedef int32_t            ssize_t;

// Pointer types for freestanding environment
typedef uint32_t           uintptr_t;
typedef int32_t            intptr_t;

// NULL definition
#ifndef NULL
#define NULL ((void*)0)
#endif

// Boolean type
typedef enum {
    false = 0,
    true = 1
} bool;

#endif // TYPES_H