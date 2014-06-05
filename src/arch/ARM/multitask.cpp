// multitask.cpp - ARM specific

#include "includes.h"
#include "arch/x86/multitask.h"
#include "arch/x86/table.h"
#include "arch/x86/irq.h"
#include "arch/x86/pic.h"
#include "core/scheduler.h"
#include "device/vga.h"

// scratch space, do not use
uint32_t syscall_arg1 = 0; // r0
uint32_t syscall_arg2 = 0; // r1
uint32_t syscall_arg3 = 0; // r2
uint32_t syscall_arg4 = 0; // r3
uint32_t syscall_arg5 = 0; // r4
uint32_t syscall_num = 0;
uint32_t as_syscall = 0;
// we have to dump registers to this array
uint8_t reg_dump_area[60];

uint32_t multitasking_enabled = 0;
uint32_t multitasking_timeslice_tick_count = MULTITASKING_RUN_TIMESLICE;
bool starting_init_process = false;

// active_tss->esp0 must be loaded upon scheduling a process to be run.

extern "C" {
    void __usermode_jump(size_t, size_t, size_t, size_t);
    void do_context_switch(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    void __process_load_registers();
    void __save_registers_non_int();
}

void cpu_regs::clear() {
    this->eax = 0;
    this->ebx = 0;
    this->ecx = 0;
    this->edx = 0;
    this->esi = 0;
    this->edi = 0;
    this->eip = 0;
    this->esp = 0;
    this->ebp = 0;
    this->cs = 0;
    this->ds = 0;
    this->es = 0;
    this->fs = 0;
    this->gs = 0;
    this->ss = 0;
    this->cr3 = 0;
    this->kernel_stack = 0;
}

/*
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
} cpu_regs;
*/

void cpu_regs::load_from_active() {
    uint32_t *ptr32 = (uint32_t*)reg_dump_area;
    uint16_t *ptr16 = (uint16_t*)reg_dump_area;
    this->eax = ptr32[0];
    this->ebx = ptr32[1];
    this->ecx = ptr32[2];
    this->edx = ptr32[3];
    this->esi = ptr32[4];
    this->edi = ptr32[5];
    this->eip = ptr32[6];
    this->eflags = ptr32[7];
    this->esp = ptr32[8];
    this->ebp = ptr32[9];
    this->cr3 = ptr32[13];
    
    this->cs = ptr16[20];
    this->ds = ptr16[21];
    this->es = ptr16[22];
    this->fs = ptr16[23];
    this->gs = ptr16[24];
    this->ss = ptr16[25];
}

void cpu_regs::load_to_active() {
    uint32_t *ptr32 = (uint32_t*)reg_dump_area;
    uint16_t *ptr16 = (uint16_t*)reg_dump_area;
    ptr32[0] = this->eax;
    ptr32[1] = this->ebx;
    ptr32[2] = this->ecx;
    ptr32[3] = this->edx;
    ptr32[4] = this->esi;
    ptr32[5] = this->edi;
    ptr32[6] = this->eip;
    ptr32[7] = this->eflags;
    ptr32[8] = this->esp;
    ptr32[9] = this->ebp;
    ptr32[13] = this->cr3;
    
    ptr16[20] = this->cs;
    ptr16[21] = this->ds;
    ptr16[22] = this->es;
    ptr16[23] = this->fs;
    ptr16[24] = this->gs;
    ptr16[25] = this->ss;
}

// a syscall(0) looks exactly like a context switch from the perspective of the switching code.
uint32_t syscall(uint32_t syscall_n, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    uint32_t ret_val;
    asm volatile("int $0x5C" : "=a" (ret_val) : "a"(syscall_n), "b"(arg1), "c"(arg2), "d"(arg3), "D"(arg4), "S"(arg5) : "memory");
    return ret_val;
}

void process_switch_immediate() {
    //kprintf("switching process from pid %u.\n", process_current->id);
    syscall(0,0,0,0,0,0);
}

uint32_t do_syscall(uint32_t syscall_n, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    if( syscall_n == 1 ) {
        return do_fork();
    }
    return ~process_current->user_regs.eax;
}

void do_context_switch(uint32_t syscall_n, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    //kprintf("Context switch!\n");
    if( (process_current == NULL) && (!starting_init_process) ) {
        // presumably, multitasking hasn't been set up yet, so just return
        return;
    }
    if(starting_init_process) {
        //kprintf("We're starting the first process. Skipping state saving.\n");
        active_tss.esp0 = process_current->regs.kernel_stack;
        active_tss.load_active();
        process_current->regs.eflags |= (1<<9);
        process_current->regs.load_to_active();
        starting_init_process = false;
        multitasking_enabled = 1;
        //kprintf("Now loading process context.\n");
        return;
    }
    if( syscall_n > 0 ) {
        process_current->in_syscall = syscall_n;
        process_current->user_regs.load_from_active();
        // If we're preempted, then the syscall's context is saved.
        // We save active_regs in a special slot to ensure that we don't lose it.
        asm volatile("sti" : : : "memory"); // we can reenable interrupts, since we're not going to the scheduler
        uint32_t ret = do_syscall( syscall_n, arg1, arg2, arg3, arg4, arg5 );
        asm volatile("cli" : : : "memory"); // make sure preemption doesn't screw this up
        process_current->user_regs.eax = ret; // set return value
        process_current->user_regs.eflags |= (1<<9); // set IF
        process_current->user_regs.load_to_active(); // load new user registers
        process_current->in_syscall = 0;
    } else {
        // Save registers
        process_current->regs.load_from_active();
        // reschedule to run
        multitasking_timeslice_tick_count = MULTITASKING_RUN_TIMESLICE;
        if( (process_current != NULL) && ((process_current->state == process_state::runnable) || (process_current->state == process_state::forking)) )
            process_add_to_runqueue( process_current );
        
        uint16_t pic_isr = pic_get_isr();
        if( pic_isr & 1 ) { // do IRQ0 code, but only if bit 0 of the collective ISR is set
            do_irq( 0, process_current->regs.eip, process_current->regs.cs ); // do_irq sends an EOI, so we don't need to do it ourselves.
            in_irq_context = true;
        }
        
        process_scheduler();
        
        if( process_current == NULL ) {
            panic("multitask: process_current is NULL during context switch!\n");
        }
        
        // load new process context
        active_tss.esp0 = process_current->regs.kernel_stack;
        active_tss.load_active();
        if( process_current->state == process_state::forking ) { // load from user_regs instead of regs (like a syscall)
            if( process_current->user_regs.eip < 0x1000 ) {
                panic("multitask: invalid process EIP!\n");
            }
            process_current->state = process_state::runnable;
            process_current->user_regs.eflags |= (1<<9); 
            process_current->user_regs.load_to_active();
        } else {
            if( process_current->regs.eip < 0x1000 ) {
                panic("multitask: invalid process EIP!\n");
            }
            process_current->regs.eflags |= (1<<9); 
            process_current->regs.load_to_active();
        }
        if( pic_isr & 1 ) { // were we in IRQ context?
            in_irq_context = false; // well, we're not going to be anymore after this
        }
    }
    // reset various state on our way out
    as_syscall = 0;
    syscall_num = 0;
    syscall_arg1 = 0;
    syscall_arg2 = 0;
    syscall_arg3 = 0;
    syscall_arg4 = 0;
    syscall_arg5 = 0;
}

void process_exec_complete( uint32_t return_value ) {
    process_current->state = process_state::dead;
    process_current->return_value = return_value;
    while(true) { process_switch_immediate(); }
    panic("Reached end of process execution but still going on!\n");
}

void initialize_multitasking(process *init) {
    active_tss.esp0 = 0;
    active_tss.ss0 = GDT_KDATA_SEGMENT*0x08; // kernel-mode SS is always the kernel data segment
    active_tss.load_active();
    
    init->id = 1;
    init->parent = 0;
    process_current = init;
}

void multitasking_start_init() {
    kprintf("Starting process 0.\n");
    active_tss.esp0 = process_current->regs.kernel_stack;
    active_tss.load_active();
    
    //process_current->regs.load_to_active();
    starting_init_process = true;
    kprintf("Switching to process.\n");
    process_switch_immediate();
}
