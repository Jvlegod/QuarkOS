#ifndef __VM_H__
#define __VM_H__
#include "ktypes.h"
#include "mem.h"

#define PTE_V 0x001UL
#define PTE_R 0x002UL
#define PTE_W 0x004UL
#define PTE_X 0x008UL
#define PTE_U 0x010UL
#define PTE_A 0x040UL
#define PTE_D 0x080UL

#define PTE_PPN_SHIFT 10

typedef uint64_t pte_t;
typedef pte_t* pagetable_t;

static pagetable_t kernel_pagetable;

void vm_init(void);
int vm_map(pagetable_t pagetable, uintptr_t va, uintptr_t pa, size_t size, uint64_t perm);
void vm_enable(pagetable_t pagetable);

#endif /* __VM_H__ */