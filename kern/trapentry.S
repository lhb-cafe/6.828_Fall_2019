/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

// question from the lab: why do we need separated functions?
// my answer: we do this to encode the trap number so that we know which trap we are handling
// there is no other way we can get the trap number using a single entry point
TRAPHANDLER(dblflt_exception, T_DBLFLT);
TRAPHANDLER(tss_exception, T_TSS);
TRAPHANDLER(segnp_exception, T_SEGNP);
TRAPHANDLER(stack_exception, T_STACK);
TRAPHANDLER(gpflt_exception, T_GPFLT);
TRAPHANDLER(pgflt_exception, T_PGFLT);
TRAPHANDLER(align_exception, T_ALIGN);

TRAPHANDLER_NOEC(division_exception, T_DIVIDE);
TRAPHANDLER_NOEC(debug_exception, T_DEBUG);
TRAPHANDLER_NOEC(nmi_exception, T_NMI);
TRAPHANDLER_NOEC(brkpt_exception, T_BRKPT);
TRAPHANDLER_NOEC(oflow_exception, T_OFLOW);
TRAPHANDLER_NOEC(bound_exception, T_BOUND);
TRAPHANDLER_NOEC(illop_exception, T_ILLOP);
TRAPHANDLER_NOEC(device_exception, T_DEVICE);
TRAPHANDLER_NOEC(fperr_exception, T_FPERR);
TRAPHANDLER_NOEC(mchk_exception, T_MCHK);
TRAPHANDLER_NOEC(simderr_exception, T_SIMDERR);

TRAPHANDLER_NOEC(syscall_exception, T_SYSCALL);

/*
 * Lab 3: Your code here for _alltraps
 */

.global _alltraps
_alltraps:
	# make %esp look like a trap frame defined in inc/trap.h
	pushl %ds
	pushl %es
	pushal

	# load GD_KD to ds and es
	movw $GD_KD, %ax
	movw %ax, %dx
	movw %ax, %es

	# call trap with a the stack pointer (trap frame) as an argument
	pushl %esp
	call trap
	# this function will not return

