#ifndef JOS_KERN_SLAB_H
#define JOS_KERN_SLAB_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/mmu.h>
#include <inc/list.h>

typedef uint16_t slab_fte_t;

#define KMEM_CACHE_NAME_MAXLEN	32
struct kmem_cache {
	DECLARE_LIST_NODE;
	list_t(struct slab) full_list, partial_list, free_list;
	uint32_t objsize;
	uint32_t obj_per_slab;
	void (*ctor)(void *kmem);
	void (*dtor)(void *kmem);
	char name[KMEM_CACHE_NAME_MAXLEN];
};

struct slab {
	DECLARE_LIST_NODE;
	uint32_t flags;
	uint32_t active;
	void *start;
	slab_fte_t *fte_index;
};

struct kmem_cache_stats {
	uint32_t full_cnt;
	uint32_t partial_cnt;
	uint32_t free_cnt;
	uint32_t page_cnt;
	uint32_t objcnt;
};


/*
 * Slab layout:
 *                     +------------------------------+  ------------------------------+
 *                     |          Slab End            |                                |
 *                     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~                                |
 *                     .                              .                                |
 *                     .                              .                                |
 *                     .                              .                                |
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|                                |
 *                     |          Slab Start          |                                |
 *               +---> +------------------------------+  --+                           |
 *               |     |          1st free offset     |    |                           |
 *               |     |         - - - - - - - - - - -|    |                        PGSIZE
 *               |     | Free     ...                 |  free table(grows down)        | 
 *               | +-> |         - - - - - - - - - - -|    |                           |
 *               | |   |          nth free offset     |    |                           |
 *               | |   +------------------------------+  --+ <-SLAB_FREE_TABLE(slab)   |
 *               | +-- |          fte_index           |    |                           |
 *               |     | - - - - - - - - - - - - - - -|    |                           |
 *               +---- |            start             |    |                           |
 *                     | - - - - - - - - - - - - - - -|  struct slab                   |
 *                     |            active            |    |                           |
 *                     | - - - - - - - - - - - - - - -|    |                           |
 *                     |            flags             |    |                           |
 *                     | - - - - - - - - - - - - - - -|    |                           |
 *                     |         list internals       |    |                           |
 *          slab---->  +------------------------------+  --+---------------------------+
 *
 * free table entry (fte):
 *    0                        11      16
 *    +-------------------------+-------+
 *    | offset from slab->start | flags |
 *    +-------------------------+-------+
 *
 */
 
#define SLAB_FREE_TABLE(slabp)	((void*)((char*)(slabp) + sizeof(struct slab)))

#define SLAB_FTE_OFFSET_SHIFT	12
#define SLAB_FTE_OFFSET(fte)	((fte) & ((1 << SLAB_FTE_OFFSET_SHIFT) - 1))
#define SLAB_FTE_FLAGS(fte) 	((fte) >> SLAB_FTE_OFFSET_SHIFT)

#define SLAB_FTE_FLAGS_CONSTRUCTED	0x1

void kmem_cache_init(void);

struct kmem_cache *kmem_cache_create(char *name, uint32_t objsize, void (*ctor)(void *kmem), void (*dtor)(void *kmem));
void kmem_cache_remove(struct kmem_cache *cache);

void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(void *kmem, struct kmem_cache *cache);

void kmem_cache_stats(struct kmem_cache_stats *stats, struct kmem_cache *cache);

#define KMEM2SLAB(kmem) (void*)PTE_ADDR((uint32_t)(kmem))

#endif /* !JOS_KERN_SLAB_H */
