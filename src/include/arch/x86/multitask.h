// multitask.h
#pragma once
#include "core/paging.h"

#define UMODE_STACK_PAGES 2
#define MULTITASKING_RUN_TIMESLICE      50

typedef struct cpu_regs {
    uint32_t eax;     // 0
    uint32_t ebx;     // 4
    uint32_t ecx;     // 8
    uint32_t edx;     // 12
    uint32_t esi;     // 16
    uint32_t edi;     // 20
    uint32_t eip;     // 24
    uint32_t eflags;  // 28
    uint32_t esp;     // 32
    uint32_t ebp;     // 36
    uint16_t cs;      // 40
    uint16_t ds;      // 42
    uint16_t es;      // 44
    uint16_t fs;      // 46
    uint16_t gs;      // 48
    uint16_t ss;      // 50
    uint32_t cr3;     // 52
    uint32_t kernel_stack; // 56
    
    void clear();
    void load_from_active();
    void load_to_active();
} cpu_regs;

extern "C" {
    extern uint32_t multitasking_enabled;
    extern uint32_t multitasking_timeslice_reset_value;
    extern uint32_t multitasking_timeslice_tick_count;
    extern uint32_t syscall_num;
    extern void __syscall_entry(void);
    extern void __ctext_switch_entry(void);
    extern void process_exec_complete(uint32_t);
    extern void __process_execution_complete(void);
}

extern void usermode_jump( size_t, size_t );
uint32_t syscall(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);