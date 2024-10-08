#include <inc/types.h>
#include <inc/assert.h>
#include <inc/error.h>

/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/env.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
  
// Commented out by lab 5
// 	if (e == curenv){
// 		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
// 	}
// 	else
// 		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);

	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	struct Env* return_env = NULL;
	int retval = env_alloc(&return_env, curenv->env_id);
	if(retval != 0){
		return -retval;
	}
	
	return_env->env_status = ENV_NOT_RUNNABLE;
	return_env->env_tf = curenv->env_tf;
	return_env->env_tf.tf_regs.reg_rax = 0;
	return return_env->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	struct Env* return_env = NULL;
	if(envid2env(envid, &return_env, 1) != 0){
		return -E_BAD_ENV;
	} else {
		if(status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE) {
			return -E_INVAL;
		}
		return_env->env_status = status;
		return 0;
	}
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	int ret;
	struct Env* env_store = NULL;
	tf->tf_eflags |= FL_IF;
	ret = envid2env(envid, &env_store, 1);
	if(ret < 0){
		return ret;
	}
	env_store->env_tf = *tf;
	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env* return_env = NULL;
	if(envid2env(envid, &return_env, 1) != 0){
		return -E_BAD_ENV;
	} else {
		return_env->env_pgfault_upcall = func;
		return 0;
	}
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
//
// An 'envid' of 0 means the page should be mapped into the current process.
//
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	if((int64_t)va >= UTOP || (((int64_t)va)%PGSIZE) !=0
		|| !(perm & PTE_P)
		|| !(perm & PTE_U)
		|| (perm & (~(PTE_P | PTE_U | PTE_AVAIL | PTE_W)))
		){
		return -E_INVAL;
	}
	struct Env* return_env;
	if(envid == 0){
		return_env = curenv;
	} else{
		return_env = NULL;
		if(envid2env(envid, &return_env, 1) != 0){
			return -E_BAD_ENV;
		} 
	}
	struct PageInfo* new_page = page_alloc(ALLOC_ZERO);
	if(new_page == NULL){
			return -E_NO_MEM;
	} else {
			if(page_insert(return_env->env_pml4e, new_page, va, perm) != 0){
				page_free(new_page);
				return -E_NO_MEM;
			} else{
				return 0;
			}
	}
		
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	if((int64_t)srcva >= UTOP 
	|| (int64_t)dstva >= UTOP 
	|| ((int64_t)srcva%PGSIZE) !=0 
	|| ((int64_t)dstva%PGSIZE) !=0){
		return -E_INVAL;
	} 
	struct Env* src_env = NULL;
	struct Env* dst_env = NULL;
	if(envid2env(srcenvid, &src_env, 1) != 0){
		return -E_BAD_ENV;
	}
	if(envid2env(dstenvid, &dst_env, 1) != 0){
		return -E_BAD_ENV;
	}
	pte_t* page_table_entry = NULL;
	struct PageInfo* src_page = page_lookup(src_env->env_pml4e, srcva, &page_table_entry);
	if(src_page == NULL){
		return -E_INVAL;
	}
	if(!(perm & PTE_P)
		|| !(perm & PTE_U)
		|| (perm & (~(PTE_P | PTE_U | PTE_AVAIL | PTE_W)))
		){
		return -E_INVAL;
	}
	if((perm&PTE_W) && (!((int64_t)(*page_table_entry)&PTE_W))){
		return -E_INVAL;
	}
	if(page_insert(dst_env->env_pml4e, src_page, dstva, perm) != 0){
		return -E_NO_MEM;
	} else{
		return 0;
	}

}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	if((int64_t)va >= UTOP || ((int64_t)va%PGSIZE) != 0){
		return -E_INVAL;
	}
	struct Env* return_env = NULL;
	if(envid2env(envid, &return_env, 1) != 0){
		return -E_BAD_ENV;
	} else{
		struct PageInfo* page_to_remove = page_lookup(return_env->env_pml4e, va, NULL);
		if(page_to_remove == NULL) {
			return 0;
		} else { 
			page_remove(return_env->env_pml4e, va);
			return 0;
		}

	}
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// When the environment is a guest (Lab 6b, aka the VMM assignment only),
// srcva should be assumed to be converted to a host virtual address (in
// the kernel address range).  You will need to add a special case to allow
// accesses from ENV_TYPE_GUEST when srcva > UTOP.
//
// Returns 0 on success, < 0 on error.
// Errors are:

//	DONE -E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	 DONE -E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	DONE -E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	DONE -E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	DONE -E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	DONE -E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	DONE -E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
		// LAB 4: Your code here.
		struct Env* targetenv = NULL;
		if(envid2env(envid, &targetenv, 0)!=0){
			return -E_BAD_ENV;
		}
		if(targetenv->env_status != 4 || targetenv->env_ipc_recving == 0){
			return -E_IPC_NOT_RECV;
		} else {
			if((int64_t)srcva < UTOP && (int64_t)srcva%PGSIZE != 0) {
				return -E_INVAL;
			}
			if((int64_t)srcva < UTOP && (!(perm & PTE_P)
			|| !(perm & PTE_U)
			|| (perm & (~(PTE_P | PTE_U | PTE_AVAIL | PTE_W)))))
			{
				return -E_INVAL;
			}
			if((int64_t)srcva < UTOP){
				pte_t* pte_store = NULL;
				struct PageInfo* srcpage = page_lookup(curenv->env_pml4e, srcva, &pte_store);
				if(srcpage == NULL){
					return -E_INVAL;
				}
				if(!((*pte_store) & PTE_W) && (perm & PTE_W)){
					return -E_INVAL;
				}
				if(page_insert(targetenv->env_pml4e, srcpage, targetenv->env_ipc_dstva, perm)!=0){
					return -E_NO_MEM;
				}
			}
			targetenv->env_ipc_recving = 0;
			targetenv->env_ipc_from = curenv->env_id;
			targetenv->env_ipc_value = value;
			targetenv->env_ipc_perm = perm;
			targetenv->env_status = ENV_RUNNABLE;
			return 0;
		}

	}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	
	if((int64_t)dstva%PGSIZE != 0 && (int64_t)dstva < UTOP){
		return -E_INVAL;
	}
	curenv->env_ipc_dstva = dstva;
	curenv->env_status = 4;
	curenv->env_ipc_recving = 1;
	return 0;
}



// A personal function to bypass our bizarre design of 4 level page table..
// Clearly the original MIT project is not designed to manage such a 4 levle page table..
static int sys_get_pte_permission(void* va){
	if((int64_t)va %4096!=0){
		panic("gotta round that mfer va ya know!\n");
	}
	pte_t* pte_store = NULL;
	page_lookup(curenv->env_pml4e, va, &pte_store);
	if(pte_store == NULL) {
		return 0;
	}
	if((*pte_store) & (PTE_U|PTE_P)) {
		return (int)((*pte_store) & (0xFFFLL));
	} else {
		return 0;
	}
	
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	return time_msec();
}



static int sys_child_mmap(envid_t srcenvid, envid_t dstenvid){
	
	int64_t PTE_COW = 0x800;
	struct Env* src_env = NULL;
	struct Env* dst_env = NULL;
	if(envid2env(srcenvid, &src_env, 1) != 0){
		return -E_BAD_ENV;
	}
	if(envid2env(dstenvid, &dst_env, 1) != 0){
		return -E_BAD_ENV;
	}
	int64_t curr_addr = 0x0;
	while(curr_addr < UTOP && curr_addr != (UXSTACKTOP - PGSIZE)) {
		pte_t* pte_store = NULL;
		struct PageInfo* page = page_lookup(src_env->env_pml4e, (void*) curr_addr, &pte_store);
		if(pte_store == NULL) {
			curr_addr += PGSIZE;
			continue;
		} else {
			int permission = (*pte_store) & (0xFFFLL);
			int perm;
			if((permission & PTE_COW) 
		      ||(permission & PTE_W)) { 
				perm = PTE_P|PTE_U|PTE_COW;
				if(page_insert(dst_env->env_pml4e, page, (void*) curr_addr, perm) != 0){
					return -E_NO_MEM;
				}
				if(page_insert(src_env->env_pml4e, page, (void*) curr_addr, perm) != 0){
					return -E_NO_MEM;
				}
			  } else {
				perm = PTE_P|PTE_U;
				if(page_insert(dst_env->env_pml4e, page, (void*) curr_addr, perm) != 0){
					return -E_NO_MEM;
				} 
			  }
			curr_addr += PGSIZE;
		}
	}
	return 0;
}

static int sys_send_packet(void* buffer, int length) {
	// if((int64_t)buffer %4096!=0){
	// 	return -E_INVAL;
	// } else if((int64_t)buffer > UTOP) {
	// 	return -E_INVAL;
	// }
	return transmit_packet(buffer, length);
}

static int sys_receive_packet(void* buffer) {
	// if((int64_t)buffer %4096!=0){
	// 	return -E_INVAL;
	// } else if((int64_t)buffer > UTOP) {
	// 	return -E_INVAL;
	// }
	return receive_packet(buffer);
}


// Dispatches to the correct kernel function, passing the arguments.
int64_t
syscall(uint64_t syscallno, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.
	
	switch (syscallno) {
		case SYS_cputs: 
			user_mem_assert((struct Env*)curenv, (void*)a1, a2, PTE_U);
			sys_cputs((char*)a1, (size_t)a2); 
			break;
		case SYS_cgetc: sys_cgetc(); break;
		case SYS_getenvid: 
			return sys_getenvid();
		case SYS_env_destroy: 
			sys_env_destroy((envid_t)a1); break;
		case SYS_yield:
			sys_yield(); break;
		case SYS_page_alloc:
			return sys_page_alloc((envid_t)a1, (void*)a2, (int)a3);
		case SYS_page_map:
			return sys_page_map(a1, (void*)a2, a3, (void*)a4, a5);
		case SYS_page_unmap:
			return sys_page_unmap(a1, (void*)a2);
		case SYS_exofork:
			return sys_exofork();
		case SYS_env_set_status:
			return sys_env_set_status(a1, a2);
		case SYS_env_set_pgfault_upcall:
			user_mem_assert((struct Env*)curenv, (void*)a2, 8, PTE_U);
			return sys_env_set_pgfault_upcall(a1, (void*) a2);
		case SYS_ipc_try_send:
			return sys_ipc_try_send(a1, a2, (void*) a3, a4);
		case SYS_ipc_recv:
			return sys_ipc_recv((void*)a1);
		case SYS_get_pte_permission:
			return sys_get_pte_permission((void*)a1);
		case SYS_child_mmap:
			return sys_child_mmap(a1, a2);
		case SYS_env_set_trapframe:
			return sys_env_set_trapframe(a1, (void*)a2);
		case SYS_time_msec:
			return sys_time_msec();
		case SYS_send_packet:
			user_mem_assert((struct Env*)curenv, (void*)a1, a2, PTE_U);
			return sys_send_packet((void*) a1, a2);
		case SYS_receive_packet:
			// user_mem_assert((struct Env*)curenv, (void*)a1, a2, PTE_U);
			return sys_receive_packet((void*) a1);
		default:
			return -E_INVAL;
		}
	return 0;
}

