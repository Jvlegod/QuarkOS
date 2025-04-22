#ifndef __MEM_H__
#define __MEM_H__
#include "list.h"

#define MEM_ALIGNMENT  4096
#define ALIGN_UP(x)    (((x) + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT - 1))
#define ALIGN_DOWN(x)  ((x) & ~(MEM_ALIGNMENT - 1))

struct mem_block {
    size_t size; // block size
    struct list_head list; // next free block
};

void mem_init(uintptr_t start, uintptr_t end);
void *mem_alloc(size_t size);
void mem_free(void *ptr);

extern char _heap_start[];
extern char _heap_end[];

#endif /* __MEM_H__ */