#ifndef JOS_KERN_BUDDY_H
#define JOS_KERN_BUDDY_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/list.h>

#define PAGE_MAX_ORDER 11

void buddy_init();

struct PageInfo *alloc_pages(int order);
void free_pages(struct PageInfo *pp, int order);

uint32_t num_free_pages_order(int order);
uint32_t num_free_pages();

#endif /* !JOS_KERN_BUDDY_H */
