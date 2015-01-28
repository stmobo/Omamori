#include "includes.h"
#include <cpuid.h>

inline void io_outb(short port, char data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

inline void io_outw(short port, short data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

inline void io_outd(short port, int data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

inline char io_inb(short port) {
    char data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

inline short io_inw(short port) {
    short data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

inline int io_ind(short port) {
    int data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

inline void io_wait() {
    asm volatile("out %%al, $0x80" : : "a"(0));
}

inline uint64_t rdtsc() {
    uint64_t tsc;
    asm volatile("rdtsc" : "=A" (tsc));
    return tsc;
}

inline void flushCache() {
    asm volatile("wbinvd" : : : "memory");
}

inline bool cpu_has_msr() {
	unsigned int a,b,c,d;
	if( __get_cpuid( 1, &a, &b, &c, &d ) ) {
		return ( d & (1<<5) );
	}
	return false;
}

inline uint64_t read_msr( uint32_t msr_no ) {
	uint32_t a = 0; // low 32
	uint32_t d = 0; // hi 32
	asm volatile( "rdmsr" : "=a"(a), "=d"(d) : "c"(msr_no) );
	uint64_t tmp = d;
	tmp <<= 32;
	tmp |= a;
	return tmp;
}

inline void write_msr( uint32_t msr_no, uint64_t value ) {
	uint32_t d = ((value >> 32)& 0xFFFFFFFF);
	uint32_t a = (value & 0xFFFFFFFF);
	asm volatile( "wrmsr" : : "a"(a), "c"(msr_no), "d"(d) );
}
