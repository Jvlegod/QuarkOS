#include "vm.h"
#include "cstdlib.h"
#include "regs.h"
#include "kprintf.h"

static pagetable_t vm_new_pagetable(void) {
    return (pagetable_t)page_alloc();
}

static inline int pt_index(uintptr_t va, int level) {
    return (va >> (12 + level * 9)) & 0x1FF;
}

static pte_t *walk_create(pagetable_t pt, uintptr_t va) {
    for (int level = 2; level > 0; level--) {
        int idx = pt_index(va, level);
        pte_t *pte = &pt[idx];
        if (*pte & PTE_V) {
            pt = (pagetable_t)(((*pte) >> PTE_PPN_SHIFT) << 12);
        } else {
            pagetable_t new_pt = vm_new_pagetable();
            if (!new_pt)
                return NULL;
            memset(new_pt, 0, PAGE_SIZE);
            *pte = (((uint64_t)new_pt >> 12) << PTE_PPN_SHIFT) | PTE_V;
            pt = new_pt;
        }
    }
    return &pt[pt_index(va, 0)];
}

int vm_map(pagetable_t pt, uintptr_t va, uintptr_t pa, size_t size, uint64_t perm) {
    for (uintptr_t a = va, p = pa; a < va + size; a += PAGE_SIZE, p += PAGE_SIZE) {
        pte_t *pte = walk_create(pt, a);
        if (!pte)
            return -1;
        *pte = ((p >> 12) << PTE_PPN_SHIFT) | perm | PTE_V | PTE_A | PTE_D;
    }
    return 0;
}

void vm_enable(pagetable_t pt) {
    uint64_t satp_val = ((uint64_t)pt >> 12) | (8ULL << 60); // Sv39
    write_csr(satp, satp_val);
    sfence_vma();
}

void vm_init(void) {
    kernel_pagetable = vm_new_pagetable();
    if (!kernel_pagetable)
        return;
    memset(kernel_pagetable, 0, PAGE_SIZE);
    // map kernel image and heap: 0x80000000 - 0x88000000 (128MB)
    // mark pages user-accessible so simple user tasks can execute
    vm_map(kernel_pagetable, 0x80000000UL, 0x80000000UL, 0x08000000UL,
           PTE_R | PTE_W | PTE_X | PTE_U);
    // map peripherals around 0x10000000
    vm_map(kernel_pagetable, 0x10000000UL, 0x10000000UL, 0x01000000UL, PTE_R|PTE_W);
    vm_enable(kernel_pagetable);
}