#ifndef JOS_INC_VMX_H
#define JOS_INC_VMX_H

#define GUEST_MEM_SZ 16 * 1024 * 1024
#define MAX_MSR_COUNT ( PGSIZE / 2 ) / ( 128 / 8 )

#ifndef __ASSEMBLER__

struct VmxGuestInfo {
	int64_t phys_sz;
	uintptr_t *vmcs;
	physaddr_t eptrt;

	// Exception bitmap.
	uint32_t exception_bmap;
	// I/O bitmap.
	uint64_t *io_bmap_a;
	uint64_t *io_bmap_b;
	// MSR load/store area.
	int msr_count;
	uintptr_t *msr_host_area;
	uintptr_t *msr_guest_area;
	int vcpunum;
};

#endif

#if defined(VMM_GUEST) || defined(VMM_HOST)

// VMCALLs
#define VMX_VMCALL_MBMAP 0x1
#define VMX_VMCALL_IPCSEND 0x2
#define VMX_VMCALL_IPCRECV 0x3
#define VMX_VMCALL_LAPICEOI 0x4
#define VMX_VMCALL_BACKTOHOST 0x5
#define VMX_VMCALL_GETDISKIMGNUM 0x6
#define VMX_VMCALL_ALLOC_CPU 0x7
#define VMX_VMCALL_GUEST_YIELD 0x8
#define VMX_VMCALL_CPUNUM 0x9

#define VMX_HOST_FS_ENV 0x1

/* VMX Capalibility MSRs */
#define IA32_FEATURE_CONTROL         0x03A
#define IA32_VMX_BASIC               0x480
#define IA32_VMX_PINBASED_CTLS       0x481
#define IA32_VMX_PROCBASED_CTLS      0x482
#define IA32_VMX_EXIT_CTLS           0x483
#define IA32_VMX_ENTRY_CTLS          0x484
#define IA32_VMX_MISC                0x485
#define IA32_VMX_CR0_FIXED0          0x486
#define IA32_VMX_CR0_FIXED1          0x487
#define IA32_VMX_CR4_FIXED0          0x488
#define IA32_VMX_CR4_FIXED1          0x489
#define IA32_VMX_VMCS_ENUM           0x48A
#define IA32_VMX_PROCBASED_CTLS2     0x48B
#define IA32_VMX_EPT_VPID_CAP        0x48C
#define IA32_VMX_TRUE_PINBASED_CTLS  0x48D
#define IA32_VMX_TRUE_PROCBASED_CTLS 0x48E
#define IA32_VMX_TRUE_EXIT_CTLS      0x48F
#define IA32_VMX_TRUE_ENTRY_CTLS     0x490
#define IA32_RTIT_CTL                0x570

// Returns true if bit x is set in value
#define BIT( val, x ) ( ( val >> x ) & 0x1 )

#endif
#endif
