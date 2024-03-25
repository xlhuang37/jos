/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>



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
	pushq $num;							\
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
	pushq $0;							\
	pushq $num;							\
	jmp _alltraps

.section .data
.global handlers





.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */
TRAPHANDLER_NOEC(handler_0, 0)
TRAPHANDLER(handler_1, 1)
TRAPHANDLER(handler_2, 2)
TRAPHANDLER_NOEC(handler_3, 3)
TRAPHANDLER(handler_4, 4)
TRAPHANDLER(handler_5, 5)
TRAPHANDLER(handler_6, 6)
TRAPHANDLER(handler_7, 7)
TRAPHANDLER(handler_8, 8)
TRAPHANDLER(handler_9, 9)
TRAPHANDLER(handler_10, 10)
TRAPHANDLER(handler_11, 11)
TRAPHANDLER(handler_12, 12)
TRAPHANDLER(handler_13, 13)
TRAPHANDLER(handler_14, 14)
TRAPHANDLER(handler_15, 15)
TRAPHANDLER(handler_16, 16)
TRAPHANDLER(handler_17, 17)
TRAPHANDLER(handler_18, 18)
TRAPHANDLER(handler_19, 19)
TRAPHANDLER(handler_20, 20)
TRAPHANDLER(handler_21, 21)
TRAPHANDLER(handler_22, 22)
TRAPHANDLER(handler_23, 23)
TRAPHANDLER(handler_24, 24)
TRAPHANDLER(handler_25, 25)
TRAPHANDLER(handler_26, 26)
TRAPHANDLER(handler_27, 27)
TRAPHANDLER(handler_28, 28)
TRAPHANDLER(handler_29, 29)
TRAPHANDLER(handler_30, 30)
TRAPHANDLER(handler_31, 31)
TRAPHANDLER(handler_32, 32)
TRAPHANDLER(handler_33, 33)
TRAPHANDLER(handler_34, 34)
TRAPHANDLER(handler_35, 35)
TRAPHANDLER(handler_36, 36)
TRAPHANDLER(handler_37, 37)
TRAPHANDLER(handler_38, 38)
TRAPHANDLER(handler_39, 39)
TRAPHANDLER(handler_40, 40)
TRAPHANDLER(handler_41, 41)
TRAPHANDLER(handler_42, 42)
TRAPHANDLER(handler_43, 43)
TRAPHANDLER(handler_44, 44)
TRAPHANDLER(handler_45, 45)
TRAPHANDLER(handler_46, 46)
TRAPHANDLER(handler_47, 47)
TRAPHANDLER_NOEC(handler_48, 48)


/*
 * Lab 3: Your code here for _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */

_alltraps:
	subq $16,%rsp  ; \
    mov %ds,8(%rsp); \
    mov %es,0(%rsp);
	PUSHA;
	mov $GD_KD, %ax  ;
    mov %ax, %ds;
    mov %ax, %es;
	mov %rsp, %rdi;
	call trap;

handlers:
    .quad handler_0
    .quad handler_1
    .quad handler_2
    .quad handler_3
    .quad handler_4
    .quad handler_5
    .quad handler_6
    .quad handler_7
    .quad handler_8
    .quad handler_9
    .quad handler_10
    .quad handler_11
    .quad handler_12
    .quad handler_13
    .quad handler_14
    .quad handler_15
    .quad handler_16
    .quad handler_17
    .quad handler_18
    .quad handler_19
    .quad handler_20
    .quad handler_21
    .quad handler_22
    .quad handler_23
    .quad handler_24
    .quad handler_25
    .quad handler_26
    .quad handler_27
    .quad handler_28
    .quad handler_29
    .quad handler_30
    .quad handler_31
    .quad handler_32
    .quad handler_33
    .quad handler_34
    .quad handler_35
    .quad handler_36
    .quad handler_37
    .quad handler_38
    .quad handler_39
    .quad handler_40
    .quad handler_41
    .quad handler_42
    .quad handler_43
    .quad handler_44
    .quad handler_45
    .quad handler_46
    .quad handler_47
    .quad handler_48