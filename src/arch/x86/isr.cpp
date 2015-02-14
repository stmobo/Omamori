#include "includes.h"
#include "arch/x86/isr.h"

extern void paging_handle_pagefault(char, uint32_t, uint32_t, uint32_t);
extern bool in_pagefault;

// when we enter these functions, our stack will look like:
// ... program stack ...
// Saved EFLAGS
// 40 - Saved CS
// 36 - Saved EIP
// 32 - Error code (or int. vector number)
// 28 - ISR function address
// 24 - EAX
// 20 - ECX
// 16 - EDX
// 12 - EBX
// 8 - Saved CS
// 4 - Saved EIP
// 0 - Error code (again) (new ESP)

void halt_err(size_t err, size_t eip, size_t cs, char* desc) {
    //panic("Kernel mode trap: %s -- error code=%#x", err, desc);
	kprintf("%s\nerror code=0x%x\nEIP=0x%x\nCS=0x%x", desc, err, eip, cs);
	logger_flush_buffer();
	while(true) {
        asm volatile("cli\n\t"
                     "hlt\n\t" : : : "memory");
    }
}

// we're calling stuff from asm, use C linkage with everything
extern "C" {
    
void do_isr_div_zero(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Division by zero error");
}
    
void do_isr_debug(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Debug interrupt");
}

void do_isr_nmi(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Non-Maskable interrupt");
}
    
void do_isr_breakpoint(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Breakpoint");
}
    
void do_isr_overflow(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Overflow error");
}
    
void do_isr_boundrange(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "BOUND instruction range exceeded");
}
    
void do_isr_invalidop(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Invalid operation error");
}
    
void do_isr_devnotavail(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "No FPU error");
}
    
void do_isr_dfault(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Double fault");
}
    
void do_isr_invalidTSS(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Invalid TSS error");
}
    
void do_isr_segnotpresent(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Segment PRESENT error");
}
    
void do_isr_stackseg(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Stack segment fault");
}

void do_isr_gpfault(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "General protection error");
}

void do_isr_pagefault(size_t err, size_t eip, size_t cs) {
    uint32_t cr2;
    asm volatile("mov %%cr2, %0" : "=g"(cr2) : : "memory");
    paging_handle_pagefault(err, cr2, eip, cs);
    in_pagefault = false;
    //halt_err(err, eip, cs, "Page fault");
}
    
void do_isr_fpexcept(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Floating point exception");
}
    
void do_isr_align(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Alignment check");
}
    
void do_isr_machine(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Machine check");
}
    
void do_isr_simd_fp(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "SIMD floating point exception");
}
    
void do_isr_virt(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Virtualization fault");
}
    
void do_isr_security(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Security fault");
}
    
void do_isr_reserved(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Reserved exception");
}

void do_isr_test(size_t err, size_t eip, size_t cs) {
    halt_err(err, eip, cs, "Test interrupt");
}

}
