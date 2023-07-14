/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


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
#define TRAPHANDLER(num)						\
.text;	\
	.globl trap_handler##num;		/* define global symbol' */	\
	.type trap_handler##num, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	trap_handler##num:			/* function starts here */		\
	.if !(num == 8 || (num >= 10 && num <= 14) || num == 17 );   \
	pushl $0;							\
	.endif;								\
	pushl $(num);							\
	jmp _alltraps;		\
.data;		\
	.long trap_handler##num;


/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

// question from the lab: why do we need separated functions?
// my answer: we do this to encode the trap number so that we know which trap we are handling
// there is no other way we can get the trap number using a single entry point

.data
.global trap_handlers
trap_handlers:
TRAPHANDLER(0)
TRAPHANDLER(1)
TRAPHANDLER(2)
TRAPHANDLER(3)
TRAPHANDLER(4)
TRAPHANDLER(5)
TRAPHANDLER(6)
TRAPHANDLER(7)
TRAPHANDLER(8)
TRAPHANDLER(9)
TRAPHANDLER(10)
TRAPHANDLER(11)
TRAPHANDLER(12)
TRAPHANDLER(13)
TRAPHANDLER(14)
TRAPHANDLER(15)
TRAPHANDLER(16)
TRAPHANDLER(17)
TRAPHANDLER(18)
TRAPHANDLER(19)
TRAPHANDLER(20)
TRAPHANDLER(21)
TRAPHANDLER(22)
TRAPHANDLER(23)
TRAPHANDLER(24)
TRAPHANDLER(25)
TRAPHANDLER(26)
TRAPHANDLER(27)
TRAPHANDLER(28)
TRAPHANDLER(29)
TRAPHANDLER(30)
TRAPHANDLER(31)
TRAPHANDLER(32)
TRAPHANDLER(33)
TRAPHANDLER(34)
TRAPHANDLER(35)
TRAPHANDLER(36)
TRAPHANDLER(37)
TRAPHANDLER(38)
TRAPHANDLER(39)
TRAPHANDLER(40)
TRAPHANDLER(41)
TRAPHANDLER(42)
TRAPHANDLER(43)
TRAPHANDLER(44)
TRAPHANDLER(45)
TRAPHANDLER(46)
TRAPHANDLER(47)
TRAPHANDLER(48)

.text

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

