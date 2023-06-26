#ifndef JOS_KERN_COMMON_H
#define JOS_KERN_COMMON_H

#define offset_of(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)

#define container_of(PTR, TYPE, MERBER) ((TYPE *)((void *)(PTR) - offset_of(TYPE, MERBER)))

#define likely(x)	(__builtin_expect(x, 1))
#define unlikely(x)	(__builtin_expect(x, 0))

#define max(a,b) \
({	typeof(a) _a = (a); \
	typeof(b) _b = (b); \
	_a > _b ? _a : _b;	\
})

#define min(a,b) \
({	typeof(a) _a = (a); \
	typeof(b) _b = (b); \
	_a < _b ? _a : _b;	\
})

#endif /* !JOS_KERN_COMMON_H */
