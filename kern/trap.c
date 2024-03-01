#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

extern uintptr_t gdtdesc_64;
struct Taskstate ts;
extern struct Segdesc gdt[];
extern long gdt_pd;
/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {0,0};

extern void handler_0();
extern void handler_1();
extern void handler_2();
extern void handler_3();
extern void handler_4();
extern void handler_5();
extern void handler_6();
extern void handler_7();
extern void handler_8();
extern void handler_9();
extern void handler_10();
extern void handler_11();
extern void handler_12();
extern void handler_13();
extern void handler_14();
extern void handler_15();
extern void handler_16();
extern void handler_17();
extern void handler_18();
extern void handler_19();
extern void handler_20();
extern void handler_21();
extern void handler_22();
extern void handler_23();
extern void handler_24();
extern void handler_25();
extern void handler_26();
extern void handler_27();
extern void handler_28();
extern void handler_29();
extern void handler_30();
extern void handler_31();
extern void handler_32();
extern void handler_33();
extern void handler_34();
extern void handler_35();
extern void handler_36();
extern void handler_37();
extern void handler_38();
extern void handler_39();
extern void handler_40();
extern void handler_41();
extern void handler_42();
extern void handler_43();
extern void handler_44();
extern void handler_45();
extern void handler_46();
extern void handler_47();
extern void handler_48();

static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	return "(unknown trap)";
}



void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	idt_pd.pd_lim = sizeof(idt)-1;
	idt_pd.pd_base = (uint64_t)idt;
	SETGATE(idt[0], 0, GD_KT, &handler_0, 0);
	SETGATE(idt[1], 0, GD_KT, &handler_1, 0);
	SETGATE(idt[2], 0, GD_KT, &handler_2, 0);
	SETGATE(idt[3], 0, GD_KT, &handler_3, 3); // BREAKPOINT
	SETGATE(idt[4], 0, GD_KT, &handler_4, 0);
	SETGATE(idt[5], 0, GD_KT, &handler_5, 0);
	SETGATE(idt[6], 0, GD_KT, &handler_6, 0);
	SETGATE(idt[7], 0, GD_KT, &handler_7, 0);
	SETGATE(idt[8], 0, GD_KT, &handler_8, 0);
	SETGATE(idt[9], 0, GD_KT, &handler_9, 0);
	SETGATE(idt[10], 0, GD_KT, &handler_10, 0);
	SETGATE(idt[11], 0, GD_KT, &handler_11, 0);
	SETGATE(idt[12], 0, GD_KT, &handler_12, 0);
	SETGATE(idt[13], 0, GD_KT, &handler_13, 0);
	SETGATE(idt[14], 0, GD_KT, &handler_14, 0);
	SETGATE(idt[15], 0, GD_KT, &handler_15, 0);
	SETGATE(idt[16], 0, GD_KT, &handler_16, 0);
	SETGATE(idt[17], 0, GD_KT, &handler_17, 0);
	SETGATE(idt[18], 0, GD_KT, &handler_18, 0);
	SETGATE(idt[19], 0, GD_KT, &handler_19, 0);
	SETGATE(idt[20], 0, GD_KT, &handler_20, 0);
	SETGATE(idt[21], 0, GD_KT, &handler_21, 0);
	SETGATE(idt[22], 0, GD_KT, &handler_22, 0);
	SETGATE(idt[23], 0, GD_KT, &handler_23, 0);
	SETGATE(idt[24], 0, GD_KT, &handler_24, 0);
	SETGATE(idt[25], 0, GD_KT, &handler_25, 0);
	SETGATE(idt[26], 0, GD_KT, &handler_26, 0);
	SETGATE(idt[27], 0, GD_KT, &handler_27, 0);
	SETGATE(idt[28], 0, GD_KT, &handler_28, 0);
	SETGATE(idt[29], 0, GD_KT, &handler_29, 0);
	SETGATE(idt[30], 0, GD_KT, &handler_30, 0);
	SETGATE(idt[31], 0, GD_KT, &handler_31, 0);
	// Software
	SETGATE(idt[32], 0, GD_KT, &handler_14, 3);
	SETGATE(idt[33], 0, GD_KT, &handler_15, 3);
	SETGATE(idt[34], 0, GD_KT, &handler_16, 3);
	SETGATE(idt[35], 0, GD_KT, &handler_17, 3);
	SETGATE(idt[36], 0, GD_KT, &handler_18, 3);
	SETGATE(idt[37], 0, GD_KT, &handler_14, 3);
	SETGATE(idt[38], 0, GD_KT, &handler_15, 3);
	SETGATE(idt[39], 0, GD_KT, &handler_16, 3);
	SETGATE(idt[40], 0, GD_KT, &handler_17, 3);
	SETGATE(idt[41], 0, GD_KT, &handler_18, 3);	
	SETGATE(idt[42], 0, GD_KT, &handler_14, 3);
	SETGATE(idt[43], 0, GD_KT, &handler_15, 3);
	SETGATE(idt[44], 0, GD_KT, &handler_16, 3);
	SETGATE(idt[45], 0, GD_KT, &handler_17, 3);
	SETGATE(idt[46], 0, GD_KT, &handler_18, 3);
	SETGATE(idt[47], 0, GD_KT, &handler_18, 3);
	// Syscall
	SETGATE(idt[48], 0, GD_KT, &handler_48, 3);

	// idt[0] = &handler_0;
	// Per-CPU setup
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;

	// Initialize the TSS slot of the gdt.
	SETTSS((struct SystemSegdesc64 *)((gdt_pd>>16)+40),STS_T64A, (uint64_t) (&ts),sizeof(struct Taskstate), 0);
	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  rip  0x%08x\n", tf->tf_rip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  rsp  0x%08x\n", tf->tf_rsp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  r15  0x%08x\n", regs->reg_r15);
	cprintf("  r14  0x%08x\n", regs->reg_r14);
	cprintf("  r13  0x%08x\n", regs->reg_r13);
	cprintf("  r12  0x%08x\n", regs->reg_r12);
	cprintf("  r11  0x%08x\n", regs->reg_r11);
	cprintf("  r10  0x%08x\n", regs->reg_r10);
	cprintf("  r9  0x%08x\n", regs->reg_r9);
	cprintf("  r8  0x%08x\n", regs->reg_r8);
	cprintf("  rdi  0x%08x\n", regs->reg_rdi);
	cprintf("  rsi  0x%08x\n", regs->reg_rsi);
	cprintf("  rbp  0x%08x\n", regs->reg_rbp);
	cprintf("  rbx  0x%08x\n", regs->reg_rbx);
	cprintf("  rdx  0x%08x\n", regs->reg_rdx);
	cprintf("  rcx  0x%08x\n", regs->reg_rcx);
	cprintf("  rax  0x%08x\n", regs->reg_rax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	print_trapframe(tf);
	switch(tf->tf_trapno){
		case T_DIVIDE: cprintf("\n\n\n\n"); break;
		case T_PGFLT: page_fault_handler(tf); break;
		case T_BRKPT: monitor(tf); return;
		case T_SYSCALL: syscall(tf->tf_regs.reg_rax
								, tf->tf_regs.reg_rdx
								, tf->tf_regs.reg_rcx
								, tf->tf_regs.reg_rbx
								, tf->tf_regs.reg_rdi
								, tf->tf_regs.reg_rsi); 
								return;
		default: break;
	}

	if(tf->tf_trapno == T_PGFLT){
		page_fault_handler(tf);
	}
	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	cprintf("trapframe is %llx\n", tf);

	//struct Trapframe *tf = &tf_;
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		assert(curenv);

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}
	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// Return to the current environment, which should be running.
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint64_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	if (tf->tf_cs == GD_KT)
		panic("Page Fault in kernel: %llx\n", fault_va);
	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_rip);
	print_trapframe(tf);
	env_destroy(curenv);
}

