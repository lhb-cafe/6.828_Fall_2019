#ifndef JOS_KERN_SLAB_TEST_H
#define JOS_KERN_SLAB_TEST_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <kern/slab.h>

void slab_test(void);

#endif /* !JOS_KERN_SLAB_TEST_H */
