// various things that I can't really put anywhere else
#pragma once
#include "core/dynmem.h"

extern char io_inb(short);
extern short io_inw(short);
extern int io_ind(short);
extern void io_outb(short, char);
extern void io_outw(short, short);
extern void io_outd(short, int);
extern void io_wait();
extern size_t strlen(char*);
extern void strcpy(char*, char*, size_t=0);
extern bool strcmp(char*, char*, size_t=0);
extern int atexit(void(*)(void*), void*);
extern void memcpy(void*, void*, size_t);
extern void memset(void*, char, size_t);
extern void memclr(void*, size_t);
extern size_t memsrch(size_t, size_t, char*, int, size_t);
extern void* kernel_end;
extern void* kernel_end_phys;
extern void* kernel_start;
extern void* kernel_start_phys;
extern uint64_t floor(double);
extern uint64_t ceil(double);
extern double fractional(double);
extern int pow(int,int);
extern uint64_t rdtsc();
extern bool interrupts_enabled();
extern "C" {
    extern uint64_t __udivmoddi4( uint64_t, uint64_t, uint64_t* );
    extern uint64_t __udivdi3( uint64_t, uint64_t );
    extern uint64_t __umoddi3( uint64_t, uint64_t );
}

#define get_return_address(var) asm volatile("mov 4(%%ebp), %0" : "=r"(var) : : "memory");

#define system_halt asm volatile("cli\n\t"\
"hlt" \
);
#define system_wait_for_interrupt() asm volatile("hlt" : : : "memory");
#define system_disable_interrupts() asm volatile("cli" : : : "memory");
#define system_enable_interrupts()  asm volatile("sti" : : : "memory");

// #define DEBUG
#define PCI_DEBUG

template <class T>
T* resize(T* alloc_to_extend, int n_elements, int desired_elements) {
    T* new_alloc = (T*)kmalloc(sizeof(T)*desired_elements);
    if( (alloc_to_extend != NULL) && (new_alloc != NULL) ) {
        for(int i=0;i<((n_elements<desired_elements) ? n_elements:desired_elements);i++) {
            new_alloc[i] = alloc_to_extend[i];
        }
        kfree(alloc_to_extend);
    }
    return new_alloc;
}

template <class T>
T* extend(T* alloc_to_extend, int n_elements) {
    return resize<T>(alloc_to_extend, n_elements, n_elements+1);
}