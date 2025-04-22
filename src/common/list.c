#include "list.h"
#include "ktypes.h"

void list_init(struct list_head *head) {
    head->prev = head;
    head->next = head;
}

void list_add(struct list_head *new, struct list_head *head) {
    new->next = head->next;
    new->prev = head;
    head->next->prev = new;
    head->next = new;
}

void list_del(struct list_head *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = entry->prev = (void *)0xDEADBEEF; // TODO: for debug
}

int list_empty(const struct list_head *head) {
    return head->next == head;
}