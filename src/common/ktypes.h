#ifndef __TYPES_H__
#define __TYPES_H__

typedef enum {
    false,
    true
} bool;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int  uint32_t;
typedef int  int32_t;
typedef unsigned long long uint64_t;
typedef long long int64_t;

typedef unsigned long int uintptr_t;
typedef unsigned long int size_t;

#define NULL (void *)0

#endif /* __TYPES_H__ */