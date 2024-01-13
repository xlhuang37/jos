
#include <inc/trap.h>
bool handle_interrupt_window(struct Trapframe *tf, struct VmxGuestInfo *ginfo, uint32_t host_vector);
bool handle_interrupts(struct Trapframe *tf, struct VmxGuestInfo *ginfo, uint32_t host_vector);
bool handle_eptviolation(struct Env *e);
bool handle_rdmsr(struct Trapframe *tf, struct VmxGuestInfo *ginfo);
bool handle_wrmsr(struct Trapframe *tf, struct VmxGuestInfo *ginfo);
bool handle_ioinstr(struct Trapframe *tf, struct VmxGuestInfo *ginfo);
bool handle_cpuid(struct Trapframe *tf, struct VmxGuestInfo *ginfo);
bool handle_vmcall(struct Trapframe *tf, struct Env *e);
