// multitask.h
#pragma once
#include "core/paging.h"

#define UMODE_STACK_PAGES 2
#define MULTITASKING_RUN_TIMESLICE      50

typedef struct cpu_regs {
    uint32_t r0;        // 0
    uint32_t r1;        // 4
    uint32_t r2;        // 8
    uint32_t r3;        // 12
    uint32_t r4;        // 16
    uint32_t r5;        // 20
    uint32_t r6;        // 24
    uint32_t r7;        // 28
    uint32_t r8;        // 32
    uint32_t r9;        // 36
    uint32_t r10;       // 40
    uint32_t r11;       // 44
    uint32_t r12;       // 48
    uint32_t r13;       // 52
    uint32_t r14;       // 56
    uint32_t r15;       // 60
    
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