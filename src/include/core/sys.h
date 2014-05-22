// various things that I can't really put anywhere else
#pragma once

extern char io_inb(short);
extern short io_inw(short);
extern int io_ind(short);
extern void io_outb(short, char);
extern void io_outw(short, short);
extern void io_outd(short, int);
extern void io_wait();
extern size_t strlen(char*);
extern void strcpy(char*, char*, size_t);
extern bool strcmp(char*, char*, size_t);
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

#define system_halt asm volatile("cli\n\t"\
"hlt" \
);
#define system_wait_for_interrupt() asm volatile("hlt" : : : "memory");
#define system_disable_interrupts() asm volatile("cli" : : : "memory");
#define system_enable_interrupts()  asm volatile("sti" : : : "memory");

// #define DEBUG
#define PCI_DEBUG