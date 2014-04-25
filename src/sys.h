// various things that I can't really put anywhere else
#pragma once

extern char io_inb(short);
extern short io_inw(short);
extern int io_inl(short);
extern void io_outb(short, char);
extern void io_outw(short, short);
extern void io_outl(short, int);
extern void io_wait();
extern char* byte_to_hex(char, char*);
extern char* short_to_hex(short, char*);
extern char* int_to_hex(int, char*);
extern char* ll_to_hex(long long int, char*);
extern char* int_to_decimal(int);
extern size_t strlen(char*);
extern int atexit(void(*)(void*), void*);
extern void memcpy(char*, char*, size_t);
extern void* kernel_end;
extern uint64_t floor(double);
extern uint64_t ceil(double);
extern double fractional(double);

extern int pow(int,int);

#define system_halt asm volatile("cli\n\t"\
"hlt" \
);
#define wait_for_interrupt() asm volatile("hlt" : : : "memory");
#define disable_interrupts() asm volatile("cli" : : : "memory");
#define enable_interrupts()  asm volatile("sti" : : : "memory");