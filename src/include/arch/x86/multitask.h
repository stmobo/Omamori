// multitask.h
#pragma once
#include "core/paging.h"

#define UMODE_STACK_PAGES 2

typedef struct cpu_regs {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t eip;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ebp;
} cpu_regs;

typedef struct process {
    cpu_regs regs;
    uint32_t *pd;
    page_frame *frames_allocated;
    uint32_t id;
    uint32_t parent;
    vaddr_range vmem_allocator;
} process;

extern "C" {
    extern cpu_regs active_regs;
    extern void __syscall_entry(void);
    extern void __ctext_switch_entry(void);
}

extern void initialize_userspace();
extern void usermode_jump( size_t, size_t );