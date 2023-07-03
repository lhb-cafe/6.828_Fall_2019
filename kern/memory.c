#include <kern/pmap.h>
#include <kern/buddy.h>
#include <kern/slab.h>
#include <kern/test/slab.h>

void
mem_init(void)
{
	// early memory init
	pmap_init();

	// the buddy system
	buddy_init();


	// initialize the slab controller
	kmem_cache_init();
	slab_test();
}