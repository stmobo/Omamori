// multitask.cpp

#include "includes.h"
#include "arch/x86/multitask.h"
#include "arch/x86/table.h"

cpu_regs active_regs;
size_t current_kern_stack = NULL;
// __syscall_entry will load this on context switch and reload from this when returning.

// Context control flow:
// Process does an int 0x5C or is preempted.
// it moves all of its registers (except %esp, %ebp, and segment registers) to active_regs.
// 

extern "C" {
    uint32_t do_syscall();
    void do_context_switch();
    void __usermode_jump(size_t func, size_t cs, size_t ds, size_t stack_addr);
}

uint32_t do_syscall() {
    kprintf("Syscall!");
    kprintf("Call number: 0x%x\n", (unsigned long long int)active_regs.eax);
    return ~active_regs.eax;
}

void do_context_switch() {
    kprintf("Ctext switch!");
}

/*
void do_context_switch( process *p ) {
    active_regs.eax = p->regs.eax;
    active_regs.ebx = p->regs.ebx;
    active_regs.ecx = p->regs.ecx;
    active_regs.edx = p->regs.edx;
    active_regs.esi = p->regs.esi;
    active_regs.edi = p->regs.edi;
    active_regs.eflags = p->regs.eflags;
    active_regs.eip = p->regs.eip;
    asm volatile("mov %0, %%cr3" : : "r"(p->pd) : "memory");
}
*/

void switch_kern_stack() {
    if( current_kern_stack != NULL ) {
        k_vmem_free( current_kern_stack );
    }
    current_kern_stack = k_vmem_alloc( UMODE_STACK_PAGES );
    active_tss.esp0 = current_kern_stack + (UMODE_STACK_PAGES*0x1000) - 1;
    active_tss.ss0 = GDT_KDATA_SEGMENT*0x08;
}

void initialize_userspace() {
    active_regs.eax = 0;
    active_regs.ebx = 0;
    active_regs.ecx = 0;
    active_regs.edx = 0;
    active_regs.esi = 0;
    active_regs.edi = 0;
    active_regs.eflags = 0;
    active_regs.eip = 0;
    
    switch_kern_stack();
}

void usermode_jump( size_t func_addr, size_t stack_addr ) {
    __usermode_jump( func_addr, GDT_UCODE_SEGMENT*0x08, GDT_UDATA_SEGMENT*0x08, stack_addr );
}