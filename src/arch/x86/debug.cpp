// debug.cpp -- Stack tracing and (possibly) symbol resolution

#include "includes.h"
#include "arch/x86/debug.h"

// EBP+8: Return address
// EBP+4: Caller EBP
// EBP: ..local variables

uint32_t *get_current_ebp() {
    uint32_t *ebp;
    asm volatile("mov %%ebp, %0" : "=r"(ebp) : : "memory");
    // *ebp == ebp
    return (uint32_t*)ebp[1]; // we're in our own function call, so we need to find the EBP of the CALLER
}

uint32_t *get_caller_ebp(uint32_t *current_ebp) {
    return (uint32_t*)current_ebp[1];
}

void stack_trace_walk(unsigned int max_levels) {
    uint32_t* ebp = (uint32_t*)(&max_levels) - 2;
    unsigned int stack_level = 0;
    kprintf("Retrieving stack trace, current EBP: %#x.\n", ebp);
    while(stack_level <= max_levels) {
        if(ebp == NULL) {
            //kprintf("debug: ebp == NULL, returning.\n");
            return;
        }
        uint32_t ret_addr = (uint32_t)ebp[1];
        if(ret_addr == 0) {
            //kprintf("debug: ret_addr == NULL, returning.\n");
            return;
        }
        kprintf("Return address of function at stack level %u: %#x\n", stack_level, ret_addr);
        ebp = (uint32_t*)(ebp[0]);
        stack_level++;
    }
}
