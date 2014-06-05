#include "includes.h"

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

inline void cpuid(uint32_t func_code, uint32_t *eax_ret, uint32_t *ebx_ret, uint32_t *ecx_ret, uint32_t *edx_ret) {
    asm volatile("cpuid"
                 : "=a"(*eax_ret), "=b"(*ebx_ret), "=c"(*ecx_ret), "=d"(*edx_ret)
                 : "a"(func_code));
}

inline void vendorid(uint32_t ret[3]) {
    asm volatile("mov $0, %%eax\n\t"
                 "cpuid\n\t"
                 : "=b"(ret[0]), "=d"(ret[1]), "=c"(ret[2])
                 : : "%eax");
}