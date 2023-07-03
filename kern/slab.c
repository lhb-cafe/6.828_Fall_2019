#include <inc/list.h>
#include <inc/string.h>
#include <kern/pmap.h>
#include <kern/buddy.h>
#include <kern/slab.h>

struct kmem_cache cache_cache;
list_t(struct kmem_cache) cache_list;

static inline uint32_t
calc_obj_per_slab(uint32_t objsize)
{
	uint32_t avail = PGSIZE - sizeof(struct slab);

	// each object occupies obj_size + sizeof(slab_fte_t)
	return avail / (objsize + sizeof(slab_fte_t));
}

// allocate a new page and initialize it with the slab header
static struct slab *
kmem_cache_new_page(uint32_t objsize)
{
	int ret;
	void *va;
	struct PageInfo *pp;
	struct slab *slabp;
	slab_fte_t *fte_it;
	uint16_t kmem_offset;

	// allocate a free physical page
	pp = alloc_pages(0);
	if (!pp) {
		return NULL;
	}

	// initialize the slab
	slabp = (struct slab*)page2kva(pp);
	slabp->flags = 0;
	slabp->active = 0;
	slabp->start = ((char*)slabp + sizeof(struct slab) + (sizeof(slab_fte_t) * calc_obj_per_slab(objsize)));
	slabp->fte_index = SLAB_FREE_TABLE(slabp);
	// initialize the slab free table
	for (fte_it = slabp->start, kmem_offset = 0;
		 (uintptr_t)fte_it < (uintptr_t)slabp->start;
		 fte_it++, kmem_offset += objsize) {
		// the slab FTE flags defaults to 0
		*fte_it = (slab_fte_t)kmem_offset;
	}

	return slabp;
}

// allocate kmem from slab
// call the constructor if necessary
static void *
kmem_cache_alloc_kmem(struct slab *slabp, void (*ctor)(void *kmem))
{
	void *kmem;
	int constructed;
	slab_fte_t *ftep = slabp->fte_index;

	// sanity check 
	if (unlikely((uintptr_t)ftep >= (uintptr_t)slabp->start)) {
		cprintf("ERROR no free memory left in slab. This shouldn't get called!\n");
		return NULL;
	}

	// get the kmem and adjust the free table index
	kmem = (char*)slabp->start + SLAB_FTE_OFFSET(*ftep);
	slabp->active++;
	// free table index grows down, so if we remove one free entry, the index increments.
	slabp->fte_index++;

	// initialize kmem if needed
	constructed = SLAB_FTE_FLAGS(*ftep) & SLAB_FTE_FLAGS_CONSTRUCTED;
	if (ctor && !constructed) {
		ctor(kmem);
	}

	return kmem;
}

static void
kmem_cache_data_init(struct kmem_cache *new, uint32_t objsize, char *name, void (*ctor)(void *kmem), void (*dtor)(void *kmem))
{
	list_init(new->full_list); 
	list_init(new->partial_list); 
	list_init(new->free_list); 
	new->objsize = objsize;
	new->obj_per_slab = calc_obj_per_slab(objsize);
	if (name) {
		strncpy(new->name, name, KMEM_CACHE_NAME_MAXLEN);
	}
	else {
		new->name[0] = '\0';
	}
	new->ctor = ctor;
	new->dtor = dtor;
}

void
kmem_cache_init(void)
{
	// kmem_cache is itself a small object which will be managed by slab
	// we have to declare one statically now before slab is ready.
	// it is used in kmem_cache_create when allocating new struct kmem_cache
	kmem_cache_data_init(&cache_cache, sizeof(struct kmem_cache), "cache_cache", NULL, NULL);

	list_init(cache_list);
	list_add(&cache_cache, cache_list);
	cprintf("kmem_cache initialized\n");
}

struct kmem_cache *
kmem_cache_create(char *name, uint32_t objsize, void (*ctor)(void *kmem), void (*dtor)(void *kmem))
{
	uint32_t obj_per_slab = calc_obj_per_slab(objsize);
	struct kmem_cache *ret;

	if (obj_per_slab <= 1) {
		cprintf("ERROR objsize %d is too large for slab, name = %s\n", objsize, name);
	}

	// allocate and init the kmem_cache object
	ret = (struct kmem_cache*)kmem_cache_alloc(&cache_cache);
	if (!ret) {
		cprintf("WARNING failed to allocate kmem_cache object, name = %s\n", name);
	}
	kmem_cache_data_init(ret, objsize, name, ctor, dtor);

	// register the object in the global list
	list_add(ret, cache_list);
	cprintf("INFO kmem_cache object '%s' successfully created\n", name);

	return ret;
}

void
kmem_cache_remove(struct kmem_cache *cache)
{
	struct kmem_cache *cache_it;
	struct slab *slabp;
	int found = 0;
	void *tmp;

	// Make sure cache is registered
	list_for_each(cache_it, cache_list) {
		if (cache_it == cache) {
			found = 1;
			break;
		}
	}
	if (!found) {
		cprintf("ERROR attempting to remove an unregistered kmem_cache object\n");
		return;
	}

	// free all allocated slabs
	list_for_each_safe(slabp, cache->full_list, tmp) {
		free_pages(kva2page(slabp), 0);
	}
	list_for_each_safe(slabp, cache->partial_list, tmp) {
		free_pages(kva2page(slabp), 0);
	}
	list_for_each_safe(slabp, cache->free_list, tmp) {
		free_pages(kva2page(slabp), 0);
	}

	// remove the kmem_cache object from the global list and free it
	list_del(cache);
	kmem_cache_free(cache, &cache_cache);
	// cache becomes invalid at this point
	// accessing its address may panic if the page is released in kmem_cache_free
}

void *
kmem_cache_alloc(struct kmem_cache *cache)
{
	struct slab *slabp;
	void *kmem;

	// get the slab, in any case, take it from its list
	if (unlikely(list_empty(cache->partial_list))) {
		if (unlikely(list_empty(cache->free_list))) {
			// no slab available in either lists, allocate a new one
			slabp = kmem_cache_new_page(cache->objsize);
			if (unlikely(!slabp)) {
				cprintf("WARNING failed to allocate slab for kmem_cache %s\n", cache->name);
				return NULL;
			}
		}
		else {
			slabp = list_pop_first(cache->free_list);
		}
	}
	else {
		slabp = list_pop_first(cache->partial_list);
	}

	// we have a slab, now allocate the kmem
	kmem = kmem_cache_alloc_kmem(slabp, cache->ctor);
	if (unlikely(!kmem)) {
		panic("ERROR failed to allocate kmem from free/partial list! name = %s\n", cache->name);
	}

	// check if the list becomes full
	if (unlikely(slabp->active == cache->obj_per_slab)) {
		list_add(slabp, cache->full_list);
	}
	else {
		list_add(slabp, cache->partial_list);
	}

	return kmem;
}

void
kmem_cache_free(void *kmem, struct kmem_cache *cache)
{
	struct slab *slabp = KMEM2SLAB(kmem);
	unsigned int kmem_offset = (uint32_t)kmem - (uint32_t)slabp->start;

	// sanity check
	if (unlikely((uintptr_t)slabp->fte_index <= (uintptr_t)SLAB_FREE_TABLE(slabp))) {
		panic("ERROR atempting to free kmem to a free slab! name = %s\n", cache->name);
	}

	// take slab away from its list no matter what and reinsert it to simplify things
	list_del(slabp);

	// call destructor if necessary
	if (cache->dtor) {
		cache->dtor(kmem);
	}

	// now free this kmem from the slab
	slabp->active--;
	slabp->fte_index--;
	// set the SLAB_FTE_FLAGS_CONSTRUCTED flag so the next allocation can skip construtor
	*slabp->fte_index = kmem_offset | (SLAB_FTE_FLAGS_CONSTRUCTED << SLAB_FTE_OFFSET_SHIFT);

	if (slabp->active == 0) {
		list_add(slabp, cache->free_list);
		// TODO: should we reclaim some pages if there are more than one free slab?
	}
	else {
		list_add(slabp, cache->partial_list);
	}
}

void
kmem_cache_stats(struct kmem_cache_stats *stats, struct kmem_cache *cache)
{
	struct slab *slabp;

	memset(stats, 0, sizeof(struct kmem_cache_stats));
	list_for_each(slabp, cache->full_list) {
		stats->full_cnt++;
		stats->objcnt += slabp->active;
	}
	list_for_each(slabp, cache->partial_list) {
		stats->partial_cnt++;
		stats->objcnt += slabp->active;
	}
	list_for_each(slabp, cache->free_list) {
		stats->free_cnt++;
	}
	stats->page_cnt = stats->full_cnt + stats->partial_cnt + stats->free_cnt;
}
