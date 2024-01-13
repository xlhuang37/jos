#include <vmm/vmx.h>
#include <vmm/vmx_asm.h>
#include <vmm/ept.h>
#include <vmm/vmexits.h>

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <kern/sched.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/kclock.h>
#include <kern/console.h>
#include <kern/spinlock.h>


void
vmx_list_vms()
{
	//findout how many VMs there
	int i;
	int vm_count = 0;
	for (i = 0; i < NENV; ++i) {
		if (envs[i].env_type == ENV_TYPE_GUEST) {
			if (vm_count == 0) {
				cprintf("Running VMs:\n");
			}
			vm_count++;
			cprintf("%d.[%x]vm%d\n", vm_count, envs[i].env_id, vm_count);
		}
	}
}

bool vmx_sel_resume(int num)
{
	int i;
	int vm_count = 0;
	for (i = 0; i < NENV; ++i) {
		if (envs[i].env_type == ENV_TYPE_GUEST) {
			vm_count++;
			if (vm_count == num) {
				cprintf("Resume vm.%d\n", num);
				envs[i].env_status = ENV_RUNNABLE;
				return true;
			}
		}
	}
	cprintf("Selected VM(No.%d VM) not found.\n", num);
	return false;
}
/* static uintptr_t *msr_bitmap; */

/* Checks VMX processor support using CPUID.
 * See Section 23.6 of the Intel manual.
 *
 * Hint: the TA solution uses the BIT() macro
 *  to simplify the implementation.
 */
bool vmx_check_support()
{
	uint32_t eax, ebx, ecx, edx;
	cpuid( 1, 0, &eax, &ebx, &ecx, &edx );
	/* Your code here */
	panic ("vmx check not implemented\n");
	cprintf("[VMM] VMX extension not supported.\n");
	return false;
}

/* This function reads the VMX-specific MSRs
 * to determine whether EPT is supported.
 * See section 24.6.2 and Appenix A.3.3 of the Intel manual.
 *
 * Hint: the TA solution uses the read_msr() helper function.
 *
 * Hint: As specified in the appendix, the values in the tables
 *  are actually offset by 32 bits.
 *
 * Hint: This needs to check two MSR bits---first verifying
 *   that secondary VMX controls are enabled, and then that
 *   EPT is available.
 */
bool vmx_check_ept()
{
	/* Your code here */
	panic ("ept check not implemented\n");
	cprintf("[VMM] EPT extension not supported.\n");
	return false;
}

/* Checks if curr_val is compatible with fixed0 and fixed1
 * (allowed values read from the MSR). This is to ensure current processor
 * operating mode meets the required fixed bit requirement of VMX.
 */
bool check_fixed_bits( uint64_t curr_val, uint64_t fixed0, uint64_t fixed1 )
{
	// TODO: Simplify this code.
	int i;
	for( i = 0 ; i < sizeof( curr_val ) * 8 ; ++i ) {
		int bit = BIT( curr_val, i );
		if ( bit == 1 ) {
			// Check if this bit is fixed to 0.
			if ( BIT( fixed1, i ) == 0 ) {
				return false;
			}
		} else if ( bit == 0 ) {
			// Check if this bit is fixed to 1.
			if ( BIT( fixed0, i ) == 1 ) {
				return false;
			}
		} else {
			assert(false);
		}
	}
	return true;
}

/*
 * Allocate a page for the VMCS region and write the VMCS Rev. ID in the first
 * 31 bits.
 */
struct PageInfo * vmx_init_vmcs()
{
	// Read the VMX_BASIC MSR.
	uint64_t vmx_basic_msr =  read_msr( IA32_VMX_BASIC );
	uint32_t vmcs_rev_id = (uint32_t) vmx_basic_msr; // Bits 30:0, Bit 31 is always 0.

	uint32_t vmcs_num_bytes =  ( vmx_basic_msr >> 32 ) & 0xfff; // Bits 44:32.
	assert( vmcs_num_bytes <= 4096 ); // VMCS can have a max size of 4096.

	//Alocate mem for VMCS region.
	struct PageInfo *p_vmxon_region = page_alloc( ALLOC_ZERO );
	if(!p_vmxon_region) {
		return NULL;
	}
	p_vmxon_region->pp_ref += 1;

	unsigned char* vmxon_region = (unsigned char *) page2kva( p_vmxon_region );
	memcpy( vmxon_region, &vmcs_rev_id, sizeof( vmcs_rev_id ) );

	return p_vmxon_region;
}

/*
 * Sets up a VMXON region and executes VMXON to put the processor in VMX root
 * operation. Returns a >=0 value if VMX root operation is achieved.
 */
int vmx_init_vmxon()
{
	//Alocate mem and init the VMXON region.
	struct PageInfo *p_vmxon_region = vmx_init_vmcs();
	if(!p_vmxon_region)
		return -E_NO_MEM;

	uint64_t cr0 = rcr0();
	uint64_t cr4 = rcr4();
	// Paging and protected mode are enabled in JOS.

	// FIXME: Workaround for CR0.NE (bochs needs this to be set to 1)
	cr0 = cr0 | CR0_NE;
	lcr0( cr0 );

	bool ret =  check_fixed_bits( cr0,
				      read_msr( IA32_VMX_CR0_FIXED0 ),
				      read_msr( IA32_VMX_CR0_FIXED1 ) );
	if ( !ret ) {
		page_decref( p_vmxon_region );
		return -E_VMX_ON;
	}
	// Enable VMX in CR4.
	cr4 = cr4 | CR4_VMXE;
	lcr4( cr4 );
	ret =  check_fixed_bits( cr4,
				 read_msr( IA32_VMX_CR4_FIXED0 ),
				 read_msr( IA32_VMX_CR4_FIXED1 ) );
	if ( !ret ) {
		page_decref( p_vmxon_region );
		return -E_VMX_ON;
	}

	// If the CET field in CR4 is set, the WP bit in CR0 must also be set
	if (BIT(cr4, 23)) {
		assert(BIT(cr0, 16));
	}

	// Ensure that IA32_FEATURE_CONTROL MSR has been properly programmed and
	// and that it's lock bit has been set.
	uint64_t feature_control = read_msr( IA32_FEATURE_CONTROL );
	if ( !BIT( feature_control, 2 )) {
		// DEP 1/14/17: qemu does not appear to properly implement a VMX-compatible BIOS.
		//   Assuming we will be running on qemu, let's just set the MSR bit in the vmm,
		//   which is how the qemu authors read the Intel manual.
		feature_control |= 0x4;
		write_msr( IA32_FEATURE_CONTROL, feature_control );
		// See if the attempt "took"
		feature_control = read_msr( IA32_FEATURE_CONTROL );
		if ( !BIT( feature_control, 2 )) {
			page_decref( p_vmxon_region );
			// VMX disabled in BIOS.
			cprintf("Unable to start VMM: VMX disabled in BIOS\n");
			return -E_NO_VMX;
		}
	}
	if ( !BIT( feature_control, 0 )) {
		// Lock bit not set, try setting it.
		feature_control |= 0x1;
		write_msr( IA32_FEATURE_CONTROL, feature_control );
	}

	uint8_t error = vmxon( (physaddr_t) page2pa( p_vmxon_region ) );
	if ( error ) {
		page_decref( p_vmxon_region );
		return -E_VMX_ON;
	}

	thiscpu->is_vmx_root = true;
	thiscpu->vmxon_region = (uintptr_t) page2kva( p_vmxon_region );

	return 0;
}

void vmcs_host_init()
{
	vmcs_write64( VMCS_HOST_CR0, rcr0() );
	vmcs_write64( VMCS_HOST_CR3, rcr3() );
	vmcs_write64( VMCS_HOST_CR4, rcr4() );

	vmcs_write16( VMCS_16BIT_HOST_ES_SELECTOR, GD_KD );
	vmcs_write16( VMCS_16BIT_HOST_SS_SELECTOR, GD_KD );
	vmcs_write16( VMCS_16BIT_HOST_DS_SELECTOR, GD_KD );
	vmcs_write16( VMCS_16BIT_HOST_FS_SELECTOR, GD_KD );
	vmcs_write16( VMCS_16BIT_HOST_GS_SELECTOR, GD_KD );
	vmcs_write16( VMCS_16BIT_HOST_CS_SELECTOR, GD_KT );

	int gd_tss = (GD_TSS0 >> 3) + thiscpu->cpu_id*2;
	vmcs_write16( VMCS_16BIT_HOST_TR_SELECTOR, gd_tss << 3 );

	uint16_t xdtr_limit;
	uint64_t xdtr_base;
	read_idtr( &xdtr_base, &xdtr_limit );
	vmcs_write64( VMCS_HOST_IDTR_BASE, xdtr_base );

	read_gdtr( &xdtr_base, &xdtr_limit );
	vmcs_write64( VMCS_HOST_GDTR_BASE, xdtr_base );

	vmcs_write64( VMCS_HOST_FS_BASE, 0x0 );
	vmcs_write64( VMCS_HOST_GS_BASE, 0x0 );
	vmcs_write64( VMCS_HOST_TR_BASE, (uint64_t) &thiscpu->cpu_ts );

	uint64_t tmpl;
	asm("movabs $.Lvmx_return, %0" : "=r"(tmpl));
	vmcs_writel(VMCS_HOST_RIP, tmpl);
}

void vmcs_guest_init()
{
	vmcs_write16( VMCS_16BIT_GUEST_CS_SELECTOR, 0x0 );
	vmcs_write16( VMCS_16BIT_GUEST_ES_SELECTOR, 0x0 );
	vmcs_write16( VMCS_16BIT_GUEST_SS_SELECTOR, 0x0 );
	vmcs_write16( VMCS_16BIT_GUEST_DS_SELECTOR, 0x0 );
	vmcs_write16( VMCS_16BIT_GUEST_FS_SELECTOR, 0x0 );
	vmcs_write16( VMCS_16BIT_GUEST_GS_SELECTOR, 0x0 );
	vmcs_write16( VMCS_16BIT_GUEST_TR_SELECTOR, 0x0 );
	vmcs_write16( VMCS_16BIT_GUEST_LDTR_SELECTOR, 0x0 );

	vmcs_write64( VMCS_GUEST_CS_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_ES_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_SS_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_DS_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_FS_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_GS_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_LDTR_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_GDTR_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_IDTR_BASE, 0x0 );
	vmcs_write64( VMCS_GUEST_TR_BASE, 0x0 );

	vmcs_write32( VMCS_32BIT_GUEST_CS_LIMIT, 0x0000FFFF );
	vmcs_write32( VMCS_32BIT_GUEST_ES_LIMIT, 0x0000FFFF );
	vmcs_write32( VMCS_32BIT_GUEST_SS_LIMIT, 0x0000FFFF );
	vmcs_write32( VMCS_32BIT_GUEST_DS_LIMIT, 0x0000FFFF );
	vmcs_write32( VMCS_32BIT_GUEST_FS_LIMIT, 0x0000FFFF );
	vmcs_write32( VMCS_32BIT_GUEST_GS_LIMIT, 0x0000FFFF );
	vmcs_write32( VMCS_32BIT_GUEST_LDTR_LIMIT, 0x0000FFFF );
	vmcs_write32( VMCS_32BIT_GUEST_TR_LIMIT, 0xFFFFF );
	vmcs_write32( VMCS_32BIT_GUEST_GDTR_LIMIT, 0x30 );
	vmcs_write32( VMCS_32BIT_GUEST_IDTR_LIMIT, 0x3FF );
	// FIXME: Fix access rights.
	vmcs_write32( VMCS_32BIT_GUEST_CS_ACCESS_RIGHTS, 0x93 );
	vmcs_write32( VMCS_32BIT_GUEST_ES_ACCESS_RIGHTS, 0x93 );
	vmcs_write32( VMCS_32BIT_GUEST_SS_ACCESS_RIGHTS, 0x93 );
	vmcs_write32( VMCS_32BIT_GUEST_DS_ACCESS_RIGHTS, 0x93 );
	vmcs_write32( VMCS_32BIT_GUEST_FS_ACCESS_RIGHTS, 0x93 );
	vmcs_write32( VMCS_32BIT_GUEST_GS_ACCESS_RIGHTS, 0x93 );
	vmcs_write32( VMCS_32BIT_GUEST_LDTR_ACCESS_RIGHTS, 0x82 );
	vmcs_write32( VMCS_32BIT_GUEST_TR_ACCESS_RIGHTS, 0x8b );

	vmcs_write32( VMCS_32BIT_GUEST_ACTIVITY_STATE, 0 );
	vmcs_write32( VMCS_32BIT_GUEST_INTERRUPTIBILITY_STATE, 0 );

	vmcs_write64( VMCS_GUEST_CR3, 0 );
	vmcs_write64( VMCS_GUEST_CR0, CR0_NE );
	vmcs_write64( VMCS_GUEST_CR4, CR4_VMXE );
	vmcs_write64( VMCS_64BIT_GUEST_LINK_POINTER, 0xffffffff );
	vmcs_write64( VMCS_64BIT_GUEST_LINK_POINTER_HI, 0xffffffff );
	vmcs_write64( VMCS_GUEST_DR7, 0x0 );
	vmcs_write64( VMCS_GUEST_RFLAGS, 0x2 );

}

static void
vmcs_ctls_init( struct Env* e )
{
	// Read the VMX_BASIC MSR.
	uint64_t vmx_basic_msr =  read_msr( IA32_VMX_BASIC );

	// Set pin based vm exec controls - a 32 bit field in the VMCS
	// The lower 32 bits of the pinbased_ctls_msr
	// say which controls may be set to zero in the VMCS, the upper
	// 32 bits say what can be turned on (set to one) in the VMCS.
	uint64_t pinbased_ctls_msr;
	// The bits in default1 class must be set: 1, 2, 4
	// We also want to enable external interrupts to cause VM exits
	uint32_t pinbased_ctls = VMCS_PIN_BASED_VMEXEC_CTL_DEFAULT1;
	//  First, we need to check bit 55 of the basic register
	if (BIT(vmx_basic_msr, 55)) {
		pinbased_ctls_msr = read_msr( IA32_VMX_TRUE_PINBASED_CTLS );
		// The low 32 bits dictate what must be 1 (or can be zero)
		pinbased_ctls |= (pinbased_ctls_msr & 0xFFFFFFFF);
	} else {
		// If bit 55 is not set, we can't set any of the other zero settings
		pinbased_ctls_msr = read_msr( IA32_VMX_PINBASED_CTLS );
	}

	// The upper 32 bits say what can be 1
	pinbased_ctls_msr >>= 32;

	// Confirm we can set the external interrupt bit
	if (pinbased_ctls_msr & VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT) {
		pinbased_ctls |= VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT;
	} else {
		cprintf("Host CPU does not support external-interrupt exiting, which is required for the JOS VMM.\n");
		assert(0);
	}

	// We don't use NMI exiting in JOS.  Confirm that this does not have to be set,
	// and check that related bits are also zero
	if (pinbased_ctls & VMCS_PIN_BASED_VMEXEC_CTL_NMI_EXITING) {
		cprintf("This CPU does not allow disabling NMI exiting; JOS does not support NMI exiting.  Please file an issue.\n");
		assert(0);
	}
	// If NMI exiting is disabled, then virtual NMIs must be 0, which means that NMI-window exiting must also be 0
	if (pinbased_ctls & VMCS_PIN_BASED_VMEXEC_CTL_VIRTUAL_NMIS) {
		cprintf("This CPU does not allow disabling virtual NMIs; JOS does not support virtual NMIs.  Please file an issue.\n");
		assert(0);
	}

	// We just haven't implemented support for posted interrupts
	if (pinbased_ctls & VMCS_PIN_BASED_VMEXEC_CTL_PROCESS_POSTED_INT) {
		cprintf("This CPU does not allow disabling posted interrupts; JOS does not support posted interrupts.  Please file an issue.\n");
		assert(0);
	}


	// If NMI exiting is disabled, then virtual NMIs must be 0, which means that NMI-window exiting must also be 0
	if (pinbased_ctls & VMCS_PIN_BASED_VMEXEC_CTL_VIRTUAL_NMIS) {
		cprintf("This CPU does not allow disabling virtual NMIs; JOS does not support virtual NMIs.  Please file an issue.\n");
		assert(0);
	}

	// We don't use VMX Preemption in JOS.
	if (pinbased_ctls & VMCS_PIN_BASED_VMEXEC_CTL_VMX_PREEMPTION_TIMER){
		cprintf("This CPU does not allow disabling VMX preemption; JOS does not support this.  Please file an issue.\n");
		assert(0);
	}

	vmcs_write32( VMCS_32BIT_CONTROL_PIN_BASED_EXEC_CONTROLS,
		      pinbased_ctls);

	// Set proc-based controls.
	uint64_t procbased_ctls_msr;
	int32_t primary_procbased_ctls;
	int use_tertiary_proc_ctls = 0;

	// Bit 55 of the vmx_basic_msr also affects the behavior of this control
	if (BIT(vmx_basic_msr, 55)) {
		procbased_ctls_msr = read_msr (IA32_VMX_TRUE_PROCBASED_CTLS);
	} else {
		procbased_ctls_msr = read_msr (IA32_VMX_PROCBASED_CTLS);
		cprintf("This hardware does not support a guest loading its own cr3, and the JOS VMM doesn't emulate this instruction yet.  Please file an issue report.\n");
		assert(0);
	}

	if (procbased_ctls_msr & VMCS_PROC_BASED_VMEXEC_CTL_ACTIVE_TERTIARY_CONTROLS)
		use_tertiary_proc_ctls = 1;

	primary_procbased_ctls = procbased_ctls_msr & 0xFFFFFFFF;


	if (!BIT(vmx_basic_msr, 55)) {
		// In this hardware configuration, the default1 class must be set
		primary_procbased_ctls |= VMCS_PROC_BASED_VMEXEC_CTL_DEFAULT1;
	}

	// Confirm that the following bits are clear
	if ((primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_CR3LOADEXIT)){
		cprintf("CPU does not support clearing the CR3 Load Exit VM Process Based vmexit control.  JOS cannot run a VMM on this hardware.\n");
		assert(0);
	}

	if ((primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_CR3STOREXIT)){
		cprintf("CPU does not support clearing the CR3 Store Exit VM Process Based vmexit control.  JOS cannot run a VMM on this hardware.\n");
		assert(0);
	}

	// We implicitly disable VMCS_PROC_BASED_VMEXEC_CTL_INVLPGEXIT; it is not clear that having it on will hurt anything
	// Put an assert here so we can debug on such hardware if we encounter it.
	// We are assuming we have EPT for JOS, which makes guest access to cr3 and invlpg ok
	if ((primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_INVLPGEXIT)){
		cprintf("CPU does not support the invlpgexit VM Process Based vmexit control.  JOS cannot run a VMM on this hardware.\n");
		assert(0);
	}

	// Ensure that the MSR bitmap is not required to be on
	if ((primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_USE_MSR_BITMAP)){
		cprintf("CPU does not support disabling the MSR bitmap control.  JOS cannot run a VMM on this hardware.\n");
		assert(0);
	}

	// Ensure that TPR shadow is not required to be on
	if ((primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_USE_TPR_SHADOW)){
		cprintf("CPU does not support disabling the TPR shadow.  JOS cannot run a VMM on this hardware.\n");
		assert(0);
	}


	// If NMI exiting is disabled, then virtual NMIs must be 0, which means that NMI-window exiting must also be 0
	if ((primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_NMI_WINDOW_EXITING)) {
		cprintf("CPU does not support disabling the NMI window exiting.  Please file an issue.\n");
		assert(0);
	}

	// Set the bits needed for JOS; confirm the hardware supports them, and they are not already mandated by the low 32 bits
	procbased_ctls_msr >>= 32;
	if (!(primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_ACTIVESECCTL)) {
		if (procbased_ctls_msr & VMCS_PROC_BASED_VMEXEC_CTL_ACTIVESECCTL) {
			primary_procbased_ctls |= VMCS_PROC_BASED_VMEXEC_CTL_ACTIVESECCTL;
		} else {
			cprintf("CPU does not support secondary VM execution controls. JOS cannot run a VMM on this hardware\n");
			assert(0);
		}
	}

	if (!(primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_HLTEXIT)) {
		if (procbased_ctls_msr & VMCS_PROC_BASED_VMEXEC_CTL_HLTEXIT) {
			primary_procbased_ctls |= VMCS_PROC_BASED_VMEXEC_CTL_HLTEXIT;
		} else {
			cprintf("CPU does not support VM exit on HLT. JOS cannot run a VMM on this hardware\n");
		}
	}
	if (!(primary_procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_USEIOBMP)) {
		if (procbased_ctls_msr & VMCS_PROC_BASED_VMEXEC_CTL_USEIOBMP) {
			primary_procbased_ctls |= VMCS_PROC_BASED_VMEXEC_CTL_USEIOBMP;
		} else {
			cprintf("CPU does not support IO bitmaps. JOS cannot run a VMM on this hardware\n");
		}
	}

	vmcs_write32( VMCS_32BIT_CONTROL_PROCESSOR_BASED_VMEXEC_CONTROLS,
		      primary_procbased_ctls);

	// Set Proc based secondary controls.
	uint64_t procbased_ctls2_msr = read_msr ( IA32_VMX_PROCBASED_CTLS2 );
	uint32_t procbased_ctls2 = 0;

	// Low 32 bits should always be 0
	assert (!(procbased_ctls2_msr & 0xFFFFFFFF));
	// Check the high 32 bits to enable EPT and unrestricted guest mode
	procbased_ctls2_msr >>= 32;
	if (procbased_ctls2_msr & VMCS_SECONDARY_VMEXEC_CTL_ENABLE_EPT) {
		procbased_ctls2 |= VMCS_SECONDARY_VMEXEC_CTL_ENABLE_EPT;
	} else {
		cprintf("CPU does not support EPT. JOS cannot run a VMM on this hardware\n");
	}

	if (procbased_ctls2_msr & VMCS_SECONDARY_VMEXEC_CTL_UNRESTRICTED_GUEST) {
		procbased_ctls2 |= VMCS_SECONDARY_VMEXEC_CTL_UNRESTRICTED_GUEST;
	} else {
		cprintf("CPU does not support unrestricted guests. JOS cannot run a VMM on this hardware\n");
	}

	// TPR shadow off implies that virtualize x2APIC, apic register virtualization, virtual interrupt delivery, and ipi virtualization are all off
	// Fortunately, these are all controlled by the secondary proc controls (except IPI virtualization -- a tertiary control) --- all of which are zeroed

	// Enable EPT.
	vmcs_write32( VMCS_32BIT_CONTROL_SECONDARY_VMEXEC_CONTROLS,
		      procbased_ctls2);

	// Initialize tertiary controls, if needed
	if (use_tertiary_proc_ctls) {
		cprintf("This CPU supports tertiary VMX controls, which JOS does not currently support.  Please file an issue.\n");
		assert(0);
	}

	// Initialize the cr3 target count; we currently do not use this feature
	vmcs_write32(VMCS_32BIT_CONTROL_CR3_TARGET_COUNT, 0);

	// Confirm PT is not active
	int pt_active = 0;
	{
		uint32_t ebx;
		cpuid( 7, 0, NULL, &ebx, NULL, NULL );
		if (BIT(ebx, 25)) {
			// Demand implement this
			cprintf("This CPU uses Intel PT; JOS VMM doesn't work under PT.  Please open a ticket\n");
			assert(0);
		}
	}


	// Set VM exit controls.  As with others, the low 32 bits of the MSR say what
	// must be on, and the upper bits say what may be on.
	uint64_t exit_ctls_msr;
	if (!BIT(vmx_basic_msr, 55)) {
		exit_ctls_msr = read_msr ( IA32_VMX_EXIT_CTLS );
	} else {
		exit_ctls_msr = read_msr ( IA32_VMX_TRUE_EXIT_CTLS );
	}

	uint32_t exit_ctls = exit_ctls_msr & 0xFFFFFFFF;

	// If bit 55 is not set, we must turn on all default 1 settings
	if (!BIT(vmx_basic_msr, 55)) {
		exit_ctls |= VMCS_VMEXIT_DEFAULT1;
	}

	// Check whether the CPU supports secondary vmexit controls
	if (BIT(exit_ctls_msr, 63)) {
		cprintf("CPU supports secondary vmexit controls, but JOS does not implement support for secondary vmexit controls.  VM launch may fail. Please file an issue.\n");
	}

	// Confirm we can not set the VMX preemption timer control
	if (exit_ctls & VMCS_VMEXIT_SAVE_VMX_PREEMPTION_TIMER) {
		cprintf("This CPU requires saving the VMX preemption timer, which JOS does not implement.  Please file an issue.\n");
		assert(0);
	}

	// Confirm we are not setting the perf global ctrl
	if (exit_ctls & VMCS_VMEXIT_LOAD_PERF_GLOBAL_CTRL) {
		cprintf("This CPU requires activating perf global controls, which JOS does not implement.  Please file an issue.\n");
		assert(0);
	}

	// Confirm we are not setting the load ia32 pat ctrl
	if (exit_ctls & VMCS_VMEXIT_LOAD_IA32_PAT) {
		cprintf("This CPU requires activating the load IA32 PAT control, which JOS does not implement.  Please file an issue.\n");
		assert(0);
	}

	// Confirm we are not setting the load CET state ctrl
	if (exit_ctls & VMCS_VMEXIT_LOAD_CET_STATE) {
		cprintf("This CPU requires activating the load IA32 CET State control, which JOS does not implement.  Please file an issue.\n");
		assert(0);
	}

	// Confirm we are not setting the load PKRS state ctrl
	if (exit_ctls & VMCS_VMEXIT_LOAD_PKRS) {
		cprintf("This CPU requires activating the load IA32 PKRS control, which JOS does not implement.  Please file an issue.\n");
		assert(0);
	}

	// Confirm we can set the flags we need
	exit_ctls_msr >>= 32;

	if (exit_ctls_msr & VMCS_VMEXIT_HOST_ADDR_SIZE) {
		exit_ctls |= VMCS_VMEXIT_HOST_ADDR_SIZE;
	} else {
		cprintf("CPU does not support setting VMEXIT host addr size. JOS cannot run a VMM on this hardware\n");
	}

	if (exit_ctls_msr & VMCS_VMEXIT_GUEST_ACK_INTR_ON_EXIT) {
		exit_ctls |= VMCS_VMEXIT_GUEST_ACK_INTR_ON_EXIT;
	} else {
		cprintf("CPU does not support setting VMEXIT guest ack intr on exit. JOS cannot run a VMM on this hardware\n");
	}

	if (exit_ctls_msr & VMCS_VMEXIT_SAVE_IA32_EFER) {
		exit_ctls |= VMCS_VMEXIT_SAVE_IA32_EFER;
	} else {
		cprintf("CPU does not support setting the save IA32 EFER MSR. JOS cannot run a VMM on this hardware\n");
	}

	if (exit_ctls_msr & VMCS_VMEXIT_LOAD_IA32_EFER) {
		exit_ctls |= VMCS_VMEXIT_LOAD_IA32_EFER;
	} else {
		cprintf("CPU does not support setting the save IA32 EFER MSR. JOS cannot run a VMM on this hardware\n");
	}

	vmcs_write32( VMCS_32BIT_CONTROL_VMEXIT_CONTROLS,
		      exit_ctls ) ;

	// We don't actually use the MSR store and load areas, yet.
	// Demand implement better checks if this changes.
	if(e->env_vmxinfo.msr_count) {
		cprintf("Internal JOS warning - the code is setting MSRs without appropriate checks implemented.  Please file an issue.\n");
		assert(0);
	}

	// Avoid setting fields we aren't using
	//vmcs_write64( VMCS_64BIT_CONTROL_VMEXIT_MSR_STORE_ADDR,
	//PADDR(e->env_vmxinfo.msr_guest_area));
	vmcs_write32( VMCS_32BIT_CONTROL_VMEXIT_MSR_STORE_COUNT,
		      e->env_vmxinfo.msr_count);
	//vmcs_write64( VMCS_64BIT_CONTROL_VMEXIT_MSR_LOAD_ADDR,
	//	      PADDR(e->env_vmxinfo.msr_host_area));
	vmcs_write32( VMCS_32BIT_CONTROL_VMEXIT_MSR_LOAD_COUNT,
		      e->env_vmxinfo.msr_count);

	// Set VM entry controls.
	uint64_t entry_ctls_msr;
	int32_t entry_ctls;
	if (!BIT(vmx_basic_msr, 55)) {
		entry_ctls_msr = read_msr(IA32_VMX_ENTRY_CTLS);
		// Set default1 class bits (0-8, 12) == 0x11FF
		entry_ctls_msr |= 0x11FF;
	} else {
		entry_ctls_msr = read_msr(IA32_VMX_TRUE_ENTRY_CTLS);
	}
	entry_ctls = entry_ctls_msr & 0xFFFFFFFF;
	entry_ctls_msr >>= 32;

	// We should load the EFER on VM entry
	if (entry_ctls_msr & VMCS_VMENTRY_LOAD_EFER) {
		entry_ctls |= VMCS_VMENTRY_LOAD_EFER;
	} else {
		cprintf("CPU does not support setting the load IA32 EFER MSR on vm entry. JOS cannot run a VMM on this hardware\n");
	}

	// Confirm that we are not setting SMM and SMI related entry controls
	if (entry_ctls & VMCS_VMENTRY_SMM) {
		cprintf("This CPU requires entry to SMM, which JOS does not implement.  Please file an issue.\n");
		assert(0);
	}

	if (entry_ctls & VMCS_VMENTRY_DEACTIVATE_DUAL) {
		cprintf("This CPU requires deactivating dual monitor treatment, which JOS does not implement.  Please file an issue.\n");
		assert(0);
	}


	// Avoid setting unused fields
	//vmcs_write64( VMCS_64BIT_CONTROL_VMENTRY_MSR_LOAD_ADDR,
	//PADDR(e->env_vmxinfo.msr_guest_area));
	vmcs_write32( VMCS_32BIT_CONTROL_VMENTRY_MSR_LOAD_COUNT,
		      e->env_vmxinfo.msr_count);

	vmcs_write32( VMCS_32BIT_CONTROL_VMENTRY_CONTROLS,
		      entry_ctls);

	// Ensure the interruption info is not initially valid
	vmcs_write32( VMCS_32BIT_CONTROL_VMENTRY_INTERRUPTION_INFO , 0);

	// Assert that EPTE_TYPE_WB is supported on this CPU
	uint64_t vpid_caps_msr = read_msr(IA32_VMX_EPT_VPID_CAP);
	if (!BIT(vpid_caps_msr, 14)) {
		cprintf("This CPU does not support EPT writeback mode, which JOS currently requires to run a VMM.  Please open an issue ticket.\n");
		assert(0);
	}

	// Ensure low 11 bits are not set in cr3
	assert (!(e->env_cr3 & (2048-1)));

	uint64_t ept_ptr = e->env_cr3 | ( ( EPT_LEVELS - 1 ) << 3 ) | EPTE_TYPE_WB;

	// Cache the ept root ptr, including flags, for use invept instruction
	e->env_vmxinfo.eptrt = ept_ptr;

	// JOS doesn't use the accessed/dirty bits in EPT right now
	/*
	  if (BIT(vpid_caps_msr, 21)) {
	  cprintf("This CPU does support EPT accessed and dirty bits\n");
	  ept_ptr |= (1<<6);
	  } else {
	  cprintf("This CPU does not support EPT accessed and dirty bits\n");
	  }
	*/

	// Zero the sysenter fields, since JOS doesn't use sysenter, for good measure
	vmcs_write64(VMCS_GUEST_IA32_SYSENTER_ESP_MSR, 0);
	vmcs_write64(VMCS_GUEST_IA32_SYSENTER_EIP_MSR, 0);

	// Set the host EFER value
	vmcs_write64( VMCS_64BIT_CONTROL_EPTPTR, ept_ptr );

	vmcs_write32( VMCS_32BIT_CONTROL_EXCEPTION_BITMAP,
		      e->env_vmxinfo.exception_bmap);
	//cprintf("Bitmap addrs are 0x%x and 0x%x\n", PADDR(e->env_vmxinfo.io_bmap_a), PADDR(e->env_vmxinfo.io_bmap_b));
	vmcs_write64( VMCS_64BIT_CONTROL_IO_BITMAP_A,
		      PADDR(e->env_vmxinfo.io_bmap_a));
	vmcs_write64( VMCS_64BIT_CONTROL_IO_BITMAP_B,
		      PADDR(e->env_vmxinfo.io_bmap_b));

}

void vmcs_dump_cpu()
{
	uint64_t flags = vmcs_readl(VMCS_GUEST_RFLAGS);

	// TODO: print all the regs.
	cprintf( "vmx: --- Begin VCPU Dump ---\n");
	cprintf( "vmx: RIP 0x%016llx RSP 0x%016llx RFLAGS 0x%016llx\n",
		 vmcs_read64( VMCS_GUEST_RIP ) , vmcs_read64( VMCS_GUEST_RSP ), flags);
	cprintf( "vmx: CR0 0x%016llx CR3 0x%016llx\n",
		 vmcs_read64( VMCS_GUEST_CR0 ), vmcs_read64( VMCS_GUEST_CR3 ) );
	cprintf( "vmx: CR4 0x%016llx \n",
		 vmcs_read64( VMCS_GUEST_CR4 ) );

	cprintf( "vmx: --- End VCPU Dump ---\n");

}

void vmexit()
{
	int exit_reason = -1;
	bool exit_handled = false;
	static uint32_t host_vector;
	// Get the reason for VMEXIT from the VMCS.
	// Your code here.

	// Check for a vm abort, which is our fault

	if (!(exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)) {

		//cprintf( "---VMEXIT Reason: %d---\n", exit_reason );
		/* vmcs_dump_cpu(); */

		switch(exit_reason & EXIT_REASON_MASK) {
		case EXIT_REASON_EXTERNAL_INT:
			host_vector = vmcs_read32(VMCS_32BIT_VMEXIT_INTERRUPTION_INFO);
			exit_handled = handle_interrupts(&curenv->env_tf, &curenv->env_vmxinfo, host_vector);
			break;
		case EXIT_REASON_INTERRUPT_WINDOW:
			exit_handled = handle_interrupt_window(&curenv->env_tf, &curenv->env_vmxinfo, host_vector);
			break;
		case EXIT_REASON_RDMSR:
			exit_handled = handle_rdmsr(&curenv->env_tf, &curenv->env_vmxinfo);
			break;
		case EXIT_REASON_WRMSR:
			exit_handled = handle_wrmsr(&curenv->env_tf, &curenv->env_vmxinfo);
			break;
		case EXIT_REASON_EPT_VIOLATION:
			exit_handled = handle_eptviolation(curenv);
			break;
		case EXIT_REASON_IO_INSTRUCTION:
			exit_handled = handle_ioinstr(&curenv->env_tf, &curenv->env_vmxinfo);
			break;
		case EXIT_REASON_CPUID:
			exit_handled = handle_cpuid(&curenv->env_tf, &curenv->env_vmxinfo);
			break;
		case EXIT_REASON_VMCALL:
			exit_handled = handle_vmcall(&curenv->env_tf, curenv);
			break;
		case EXIT_REASON_HLT:
			cprintf("\nHLT in guest, exiting guest.\n");
			env_destroy(curenv);
			exit_handled = true;
			break;
		}
	} else {
		// VM Abort
		uint32_t abort_reason = vmcs_read32(VMCS_16BIT_CONTROL_EPTP_INDEX);
		cprintf("VM Abort for hardware misconfiguration, 0x%x.  Uh oh\n", abort_reason);

		cprintf( "---VMEXIT Reason: %d %x---\n", exit_reason, exit_reason );
		vmcs_dump_cpu();
	}

	if(!exit_handled) {
		cprintf( "Unhandled VMEXIT, reason 0x%x, aborting guest.\n", exit_reason );
		vmcs_dump_cpu();
		env_destroy(curenv);
	}

	sched_yield();
}

void asm_vmrun(struct Trapframe *tf)
{
	/* cprintf("VMRUN\n"); */
	// NOTE: Since we re-use Trapframe structure, tf.tf_err contains the value
	// of cr2 of the guest.
	tf->tf_ds = curenv->env_runs;
	tf->tf_es = 0;
	unlock_kernel();
	asm(
		"push %%rdx; push %%rbp;"
		"push %%rcx \n\t" /* placeholder for guest rcx */
		"push %%rcx \n\t"
		/* Set the VMCS rsp to the current top of the frame. */
		/* Your code here */
		"1: \n\t"
		/* Reload cr2 if changed */
		"mov %c[cr2](%0), %%rax \n\t"
		"mov %%cr2, %%rdx \n\t"
		"cmp %%rax, %%rdx \n\t"
		"je 2f \n\t"
		"mov %%rax, %%cr2 \n\t"
		"2: \n\t"
		/* Check if vmlaunch of vmresume is needed, set the condition code
		 * appropriately for use below.
		 *
		 * Hint: We store the number of times the VM has run in tf->tf_ds
		 *
		 * Hint: In this function,
		 *       you can use register offset addressing mode, such as '%c[rax](%0)'
		 *       to simplify the pointer arithmetic.
		 */
		/* Your code here */
		/* Load guest general purpose registers from the trap frame.  Don't clobber flags.
		 *
		 */
		/* Your code here */
		/* Enter guest mode */
		/* Your code here:
		 *
		 * Test the condition code from rflags
		 * to see if you need to execute a vmlaunch
		 * instruction, or just a vmresume.
		 *
		 * Note: be careful in loading the guest registers
		 * that you don't do any compareison that would clobber the condition code, set
		 * above.
		 */
		".Lvmx_return: "

		/* POST VM EXIT... */
		"mov %0, %c[wordsize](%%rsp) \n\t"
		"pop %0 \n\t"
		/* Save general purpose guest registers and cr2 back to the trapframe.
		 *
		 * Be careful that the number of pushes (above) and pops are symmetrical.
		 */
		/* Your code here */
		"pop  %%rbp; pop  %%rdx \n\t"

		"setbe %c[fail](%0) \n\t"
		: : "c"(tf), "d"((unsigned long)VMCS_HOST_RSP),
		  [launched]"i"(offsetof(struct Trapframe, tf_ds)),
		  [fail]"i"(offsetof(struct Trapframe, tf_es)),
		  [rax]"i"(offsetof(struct Trapframe, tf_regs.reg_rax)),
		  [rbx]"i"(offsetof(struct Trapframe, tf_regs.reg_rbx)),
		  [rcx]"i"(offsetof(struct Trapframe, tf_regs.reg_rcx)),
		  [rdx]"i"(offsetof(struct Trapframe, tf_regs.reg_rdx)),
		  [rsi]"i"(offsetof(struct Trapframe, tf_regs.reg_rsi)),
		  [rdi]"i"(offsetof(struct Trapframe, tf_regs.reg_rdi)),
		  [rbp]"i"(offsetof(struct Trapframe, tf_regs.reg_rbp)),
		  [r8]"i"(offsetof(struct Trapframe, tf_regs.reg_r8)),
		  [r9]"i"(offsetof(struct Trapframe, tf_regs.reg_r9)),
		  [r10]"i"(offsetof(struct Trapframe, tf_regs.reg_r10)),
		  [r11]"i"(offsetof(struct Trapframe, tf_regs.reg_r11)),
		  [r12]"i"(offsetof(struct Trapframe, tf_regs.reg_r12)),
		  [r13]"i"(offsetof(struct Trapframe, tf_regs.reg_r13)),
		  [r14]"i"(offsetof(struct Trapframe, tf_regs.reg_r14)),
		  [r15]"i"(offsetof(struct Trapframe, tf_regs.reg_r15)),
		  [cr2]"i"(offsetof(struct Trapframe, tf_err)),
		  [wordsize]"i"(sizeof(uint64_t))
                : "cc", "memory"
		  , "rax", "rbx", "rdi", "rsi"
		  , "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
		);
	lock_kernel();
	if(tf->tf_es) {
		uint32_t exit_reason = vmcs_read32(VMCS_32BIT_INSTRUCTION_ERROR);
		cprintf("Error during VMLAUNCH/VMRESUME\n");
		switch(exit_reason) {
		case VMLAUNCH_FAIL_INVALID_CONTROL_FIELDS :
			cprintf("Attempt to launch a VM with invalid control fields.\n");
			break;
		case VMLAUNCH_FAIL_INVALID_HOST_STATE_FIELDS :
			cprintf("Attempt to launch a VM with invalid host state fields.\n");
			break;
		default:
			cprintf("Failure code %d, see Intel SDM manual\n", exit_reason);
		}
	} else {
		curenv->env_tf.tf_rsp = vmcs_read64(VMCS_GUEST_RSP);
		curenv->env_tf.tf_rip = vmcs_read64(VMCS_GUEST_RIP);
		vmexit();
	}
}

void
msr_setup(struct VmxGuestInfo *ginfo)
{
	struct vmx_msr_entry *entry;
	uint32_t idx[] = { EFER_MSR };
	int i, count = sizeof(idx) / sizeof(idx[0]);
	int skipped = 0;

	assert(count <= MAX_MSR_COUNT);

	for(i=0; i<count; ++i) {
		uint64_t value = read_msr(idx[i]);

		// DEP 12/12/23: EFER is a special case, and goes in the main VMCS, not
		// the standard MSR area.
		if ( idx[i] == EFER_MSR) {
			// Set host value
			vmcs_write64(VMCS_64BIT_HOST_IA32_EFER, value);

			// Disable LMA upon boot
			value &= ~(1 << EFER_LMA);
			vmcs_write64(VMCS_64BIT_GUEST_IA32_EFER, value);

			skipped++;
		} else {

			entry = ((struct vmx_msr_entry *)ginfo->msr_host_area) + i;
			entry->msr_index = idx[i];
			entry->msr_value = value;

			entry = ((struct vmx_msr_entry *)ginfo->msr_guest_area) + i;
			entry->msr_index = idx[i];
		}
	}

	count -= skipped;
	assert(count >= 0);
	ginfo->msr_count = count;

}

void
bitmap_setup(struct VmxGuestInfo *ginfo)
{
	unsigned int io_ports[] = { IO_RTC, IO_RTC+1 };
	int i, count = sizeof(io_ports) / sizeof(io_ports[0]);

	for(i=0; i<count; ++i) {
		int idx = io_ports[i] / (sizeof(uint64_t) * 8);
		if(io_ports[i] < 0x7FFF) {
			ginfo->io_bmap_a[idx] |= ((0x1uL << (io_ports[i] & 0x3F)));
		} else if (io_ports[i] < 0xFFFF) {
			ginfo->io_bmap_b[idx] |= ((0x1uL << (io_ports[i] & 0x3F)));
		} else {
			assert(false);
		}
	}
}

/*
 * Processor must be in VMX root operation before executing this function.
 */
int vmx_vmrun( struct Env *e )
{

	if ( e->env_type != ENV_TYPE_GUEST ) {
		return -E_INVAL;
	}
	uint8_t error;

	if( e->env_runs == 1 ) {
		physaddr_t vmcs_phy_addr = PADDR(e->env_vmxinfo.vmcs);

		// Call VMCLEAR on the VMCS region.
		error = vmclear(vmcs_phy_addr);
		// Check if VMCLEAR succeeded. ( RFLAGS.CF = 0 and RFLAGS.ZF = 0 )
		if ( error )
			return -E_VMCS_INIT;

		// Make this VMCS working VMCS.
		error = vmptrld(vmcs_phy_addr);
		if ( error )
			return -E_VMCS_INIT;

		vmcs_host_init();
		vmcs_guest_init();
		// Setup IO and exception bitmaps.
		bitmap_setup(&e->env_vmxinfo);
		// Setup the msr load/store area
		msr_setup(&e->env_vmxinfo);
		vmcs_ctls_init(e);

	} else {
		// Make this VMCS working VMCS.
		error = vmptrld(PADDR(e->env_vmxinfo.vmcs));
		if ( error ) {
			return -E_VMCS_INIT;
		}
	}

	vmcs_write64( VMCS_GUEST_RSP, curenv->env_tf.tf_rsp  );
	vmcs_write64( VMCS_GUEST_RIP, curenv->env_tf.tf_rip );
	panic ("asm vmrun incomplete\n");
	asm_vmrun( &e->env_tf );
	return 0;
}
