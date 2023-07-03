#include <inc/string.h>
#include <inc/list.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/buddy.h>

struct free_area {
	list_t(struct PageInfo) free_list[PAGE_MAX_ORDER];
	uint32_t free_cnt[PAGE_MAX_ORDER];
	uint32_t num_free_pages; // number of free base pages
};

struct free_area kern_free_area;

// linux ffs() is arch specific, for now we implement the stupid c way
// unlike its linux counterpart, first index starts at 0
// return 32 if input is 0
static inline int
find_first_set_bit(uint32_t input) {
	int i;
	for (i = 0; i < 4; i++, input >> 8) {
		switch (input & 0xF) {
		case 1:
		case 3:
		case 5:
		case 7:
		case 9:
		case 11:
		case 13:
		case 15:
			return 8 * i;
		case 2:
		case 6:
		case 10:
		case 14:
			return 8 * i + 1;
		case 4:
		case 12:
			return 8 * i + 2;
		case 8:
			return 8 * i + 3;
		}
	}
	return 32; // input is 0
}

static inline void
set_page_head(struct PageInfo *pp, int order)
{
	pp->pp_flags &= ~(PAGE_TYPE_MASK | PAGE_ORDER_MASK);
	pp->pp_flags |= (PAGE_TYPE_COMP_HEAD | (order << PAGE_ORDER_SHIFT));
}

static inline void
set_page_tail(struct PageInfo *pp)
{
	pp->pp_flags &= ~PAGE_TYPE_MASK;
	pp->pp_flags |= PAGE_TYPE_COMP_TAIL;
}

static inline int
get_page_order(struct PageInfo *pp)
{
	return (pp->pp_flags & PAGE_ORDER_MASK) >> PAGE_ORDER_SHIFT;
}

// this simply puts the page into free_list without checking for
// buddy to combine
static inline void
__insert_to_free_list(struct PageInfo *pp, struct free_area *area)
{
	int order = get_page_order(pp);
	list_add(pp, area->free_list[get_page_order(pp)]);
	area->free_cnt[order]++;
	area->num_free_pages += (1 << order);
}

static inline void
__remove_from_free_list(struct PageInfo *pp, struct free_area *area)
{
	int order = get_page_order(pp);
	list_del(pp);
	area->free_cnt[order]--;
	area->num_free_pages -= (1 << order);
}

static void
insert_free_list(uint32_t start, uint32_t end, struct free_area *area)
{
	int order;
	struct PageInfo *pp;

	while (start <= end) {
		// We design the buddy system such that the compound pages are aligned
		// in memory with respect to their order.
		// i.e.,
		// 	an n-th order page mush have at least n trailing 0's in its pfn
		// This makes the buddy algorithm a lot easier (from software perspective)
		order = min(PAGE_MAX_ORDER - 1, find_first_set_bit(start));
		// [start, end]
		while (start + (1 << order) > end + 1) {
			order--;
		}

		pp = pfn_to_page(start);
		set_page_head(pp, order);
		// we don't bother removing the page from page_free_list
		// because page_free_list will never be used from this point forward
		//cprintf("DEBUG insert a new %d-order page starting at 0x%x\n", order, page2pa(pp));
		__insert_to_free_list(pp, area);
		start += (1 << order);
		// no need to initialize page tails because they should be initialized
		// to 0 already during early memory init in pmap
	}
}

// for now the kernel can only map 256MB memory at most
#define MAX_NORMAL_PFN 0x10000
static void
__buddy_init(struct free_area *area)
{
	int i;
	struct PageInfo *pp;
	uint32_t pfn, start, end;

	memset(area, 0, sizeof(struct free_area));

	// initialize list heads
	for (i = 0; i < PAGE_MAX_ORDER; i++) {
		list_init(area->free_list[i]);
	}

	// scan the pages to initialize the free_list
	for (pfn = 0; pfn < npages && pfn < MAX_NORMAL_PFN; pfn++) {
		pp = pfn_to_page(pfn);

		// get next free page
		if (pp->pp_ref) {
			continue;
		}
		start = pfn;

		// get the last contiguous free page 
		for (; pfn < npages - 1; pfn++) {
			pp = pfn_to_page(pfn + 1);
			if (pp->pp_ref == 0) {
				continue;
			}
			break;
		}
		end = pfn;

		insert_free_list(start, end, area);
	}
}
#undef MAX_NORMAL_PFN

void
buddy_init()
{
	__buddy_init(&kern_free_area);
	cprintf("INFO buddy system initialized, total free = %u\n", num_free_pages());
}

static inline void
page_ref(struct PageInfo *pp)
{
	// for now it suffices to only mark the first page
	pp->pp_ref++;
}

static inline uint16_t
page_deref(struct PageInfo *pp)
{
	// for now it suffices to only mark the first page
	assert(pp->pp_ref != 0);
	return --pp->pp_ref;
}

static struct PageInfo *
get_page_from_freelist(int order, struct free_area *area)
{
	int cur_order = order;
	struct PageInfo *pp;

	// fast path
	if (!list_empty(area->free_list[order])) {
		// delete pp from free_list and return it
		pp = list_first_entry(area->free_list[order]);
		__remove_from_free_list(pp, area);
		return pp;
	}

	// find available order from the free_list
	while (cur_order < PAGE_MAX_ORDER && list_empty(area->free_list[cur_order])) {
		cur_order++;
	}
	if (cur_order == PAGE_MAX_ORDER) {
		return NULL;
	}

	pp = list_first_entry(area->free_list[cur_order]);
	__remove_from_free_list(pp, area);
	while (cur_order > order) {
		// split the page into two buddies
		set_page_head(pp, --cur_order);
		// insert one buddy back into free_list
		__insert_to_free_list(pp, area);
		// pp points to the the other buddy
		pp += (1 << cur_order);
	}

	return pp;
}

struct PageInfo *
alloc_pages(int order)
{
	struct PageInfo *pp;
	pp = get_page_from_freelist(order, &kern_free_area);
	if (pp) {
		page_ref(pp);
	}
	return pp;
}

static inline uint32_t
find_buddy_pfn(uint32_t pfn, int order)
{
	return pfn ^ (1 << order);
}

#define find_buddy_page(pp, order) pfn_to_page(find_buddy_pfn(page_to_pfn(pp), order))

static void
put_page_to_freelist(struct PageInfo *pp, int order, struct free_area *area)
{
	struct PageInfo *buddy;

	// recursively combine available buddies
	while (order < PAGE_MAX_ORDER - 1) {
		buddy = find_buddy_page(pp, order);
		// Is it possible that 'buddy' is inside another allocated higher-ordered
		// page, in which 'buddy' is not the first page of the compound page, thus
		// its own pp_ref does not reflect the actual ref count?
		// The answer is no. we know that 'pp' was allocated, and 'buddy' is its
		// buddy, there is no way 'buddy' can be allocated in a larger compound page
		// while its buddy is not in it without breaking the alignment rule.
		if (page_is_free(buddy) && get_page_order(buddy) == order) {
			// delete 'buddy' from the free_list
			__remove_from_free_list(buddy, area);
			// combine the pages
			if (pp > buddy) {
				set_page_tail(buddy);
			}
			else {
				set_page_tail(pp);
				pp = buddy;
			}
			order++;
		}
		else {
			break;
		}
	}
	// insert the new page back to free_list
	set_page_head(pp, order);
	__insert_to_free_list(pp, area);
}
                                                                                                               
void
free_pages(struct PageInfo *pp, int order)
{
	if (page_deref(pp) == 0) {
		put_page_to_freelist(pp, order, &kern_free_area);
	}
}

uint32_t
num_free_pages_order(int order)
{
	return kern_free_area.free_cnt[order];
}

uint32_t num_free_pages()
{
	return kern_free_area.num_free_pages;
}
