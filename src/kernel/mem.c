#include "mem.h"
#include "uart.h"

LIST_HEAD(free_list);

void mem_init(uintptr_t start, uintptr_t end) {
    start = ALIGN_UP(start + sizeof(struct mem_block));
    end = ALIGN_DOWN(end);

    if (start >= end) return;

    struct mem_block *blk = (struct mem_block *)start;
    blk->size = end - start;
    list_add(&blk->list, &free_list);
}

// temp first fit here
// TODO: could we impl other mehtod?
void *mem_alloc(size_t size) {
    size = ALIGN_UP(size + sizeof(struct mem_block));

    struct list_head *pos;
    for (pos = free_list.next; pos != &free_list; pos = pos->next) {
        struct mem_block *blk = container_of(pos, struct mem_block, list);
        
        if (blk->size >= size) {
            list_del(pos);

            if (blk->size > size + sizeof(struct mem_block)) {
                struct mem_block *new = (struct mem_block *)((char *)blk + size);
                new->size = blk->size - size;
                // insert remaining block keeping address order
                struct list_head *p;
                for (p = free_list.next; p != &free_list; p = p->next) {
                    struct mem_block *curr = container_of(p, struct mem_block, list);
                    if (curr > new) break;
                }
                new->list.next = p;
                new->list.prev = p->prev;
                p->prev->next = &new->list;
                p->prev = &new->list;
                blk->size = size;
            }

            return (void *)(blk + 1);
        }
    }
    return NULL;
}

void mem_free(void *ptr) {
    if (!ptr) return;

    struct mem_block *blk = (struct mem_block *)ptr - 1;
    struct list_head *pos;

    for (pos = free_list.next; pos != &free_list; pos = pos->next) {
        struct mem_block *curr = container_of(pos, struct mem_block, list);
        if (curr > blk) break;
    }

    blk->list.next = pos;
    blk->list.prev = pos->prev;
    pos->prev->next = &blk->list;
    pos->prev = &blk->list;

    if (blk->list.prev != &free_list) {
        struct mem_block *prev = container_of(blk->list.prev, struct mem_block, list);
        if ((char *)prev + prev->size == (char *)blk) {
            prev->size += blk->size;
            list_del(&blk->list);
            blk = prev;
        }
    }

    if (blk->list.next != &free_list) {
        struct mem_block *next = container_of(blk->list.next, struct mem_block, list);
        if ((char *)blk + blk->size == (char *)next) {
            blk->size += next->size;
            list_del(&next->list);
        }
    }
}