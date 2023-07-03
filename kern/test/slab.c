#include <inc/assert.h>
#include <inc/assert.h>

#include <kern/buddy.h>
#include <kern/slab.h>
#include <kern/test/slab.h>

#define TEST_SIZE_0 20

static struct kmem_cache *test_cache;
static struct kmem_cache_stats stats0, stats1;
static int ctor_called, dtor_called;
static void *kmem[1000];

static void
test_ctor0(void *obj)
{
	char *cur = obj;
	int i;

	for (i = 0; i < TEST_SIZE_0; i++) {
		*cur++ = (char)i;
	}
	ctor_called++;
}

static void
test_dtor0(void *obj)
{
	((char*)obj)[TEST_SIZE_0-1] = (char)(TEST_SIZE_0 - 1);
	dtor_called++;
}

void
slab_test(void)
{
	void *va;
	int i, j;
	int size;
	struct PageInfo *pp;
	int freepages0, freepages1;

	// test kmem_cache_create
	ctor_called = 0;
	dtor_called = 0;
	test_cache = kmem_cache_create("test_cache", TEST_SIZE_0, test_ctor0, test_dtor0);
	assert(ctor_called == 0);
	assert(dtor_called == 0);
	freepages0 = num_free_pages();

	// test kmem_cache_alloc
	kmem[0] = kmem_cache_alloc(test_cache);
	assert(kmem[0]);
	assert(ctor_called == 1);
	assert(dtor_called == 0);
	for (i = 0; i < TEST_SIZE_0; i++) {
		assert(((char*)kmem[0])[i] == (char)i);
	}

	// modify the kmem and test kmem_cache_free
	va = kmem[0];
	((char*)kmem[0])[TEST_SIZE_0 - 1] = (char)0;
	kmem_cache_free(kmem[0], test_cache);
	assert(ctor_called == 1);
	assert(dtor_called == 1);

	// allocate the kmem again, test if it is the same address, and test the content
	kmem[0] = kmem_cache_alloc(test_cache);
	assert(kmem[0] == va);
	assert(ctor_called == 1); // should not need to construct again
	assert(dtor_called == 1);
	for (i = 0; i < TEST_SIZE_0; i++) {
		assert(((char*)kmem[0])[i] == (char)i);
	}

	// allocate 4 more, test they are from the same slab
	kmem[1] = kmem_cache_alloc(test_cache);
	kmem[2] = kmem_cache_alloc(test_cache);
	kmem[3] = kmem_cache_alloc(test_cache);
	kmem[4] = kmem_cache_alloc(test_cache);
	assert(KMEM2SLAB(kmem[1]) == KMEM2SLAB(va));
	assert(KMEM2SLAB(kmem[2]) == KMEM2SLAB(va));
	assert(KMEM2SLAB(kmem[3]) == KMEM2SLAB(va));
	assert(KMEM2SLAB(kmem[4]) == KMEM2SLAB(va));

	// stress test
	kmem_cache_free(kmem[0], test_cache);
	kmem_cache_free(kmem[1], test_cache);
	kmem_cache_free(kmem[2], test_cache);
	kmem_cache_free(kmem[3], test_cache);
	kmem_cache_free(kmem[4], test_cache);
	for (i = 0; i < 1000; i++) {
		kmem[0] = kmem_cache_alloc(test_cache);
		kmem[1] = kmem_cache_alloc(test_cache);
		kmem[2] = kmem_cache_alloc(test_cache);
		kmem[3] = kmem_cache_alloc(test_cache);
		kmem[4] = kmem_cache_alloc(test_cache);
		kmem_cache_free(kmem[0], test_cache);
		kmem_cache_free(kmem[1], test_cache);
		kmem_cache_free(kmem[2], test_cache);
		kmem_cache_free(kmem[3], test_cache);
		kmem_cache_free(kmem[4], test_cache);
	}
	assert(KMEM2SLAB(kmem[0]) == KMEM2SLAB(va));
	assert(KMEM2SLAB(kmem[1]) == KMEM2SLAB(va));
	assert(KMEM2SLAB(kmem[2]) == KMEM2SLAB(va));
	assert(KMEM2SLAB(kmem[3]) == KMEM2SLAB(va));
	assert(KMEM2SLAB(kmem[4]) == KMEM2SLAB(va));
	assert(ctor_called == 5);

	// stress test more
	for (j = 0; j < 1000; j++) {
		kmem[j] = kmem_cache_alloc(test_cache);
	}
	kmem_cache_stats(&stats0, test_cache);
	assert(stats0.objcnt == 1000);
	for (j = 0; j < 1000; j++) {
		kmem_cache_free(kmem[j], test_cache);
	}
	kmem_cache_stats(&stats1, test_cache);
	assert(stats1.objcnt == 0);
	assert(stats1.page_cnt == stats0.page_cnt);
	for (i = 0; i < 100; i++) {
		for (j = 0; j < 1000; j++) {
			kmem[j] = kmem_cache_alloc(test_cache);
		}
		for (j = 0; j < 1000; j++) {
			kmem_cache_free(kmem[j], test_cache);
		}
	}
	kmem_cache_stats(&stats1, test_cache);
	freepages1 = num_free_pages();
	assert(stats1.objcnt == 0);
	assert(stats1.page_cnt == stats0.page_cnt);
	assert(freepages0 == freepages1 + stats0.page_cnt);

	// test kmem_cache_remove
	kmem_cache_remove(test_cache);
	freepages1 = num_free_pages();
	assert(freepages0 == freepages1);

	cprintf("slab_test succeeded!\n");
}
