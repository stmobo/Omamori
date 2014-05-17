// sys.cpp - miscellaneous system functions

#include "includes.h"
#include "core/sys.h"
#include "device/vga.h"
#include "core/dynmem.h"

void *__stack_chk_guard = NULL;

void io_outb(short port, char data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

void io_outw(short port, short data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

void io_outd(short port, int data) {
    asm volatile("out %0, %1\n\t" : : "a"(data), "d"(port) );
    return;
}

char io_inb(short port) {
    char data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

short io_inw(short port) {
    short data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

int io_ind(short port) {
    int data;
    asm volatile("in %1, %0\n\t" : "=a"(data) : "d"(port) );
    return data;
}

void io_wait() {
    asm volatile("out %%al, $0x80" : : "a"(0));
}

unsigned long long int rdtsc() {
    unsigned long long int tsc;
    asm volatile("rdtsc" : "=A" (tsc));
    return tsc;
}

void *operator new(size_t size) {
    return kmalloc(size);
}

void *operator new[](size_t size) {
    return kmalloc(size);
}

void operator delete(void* ptr) {
    kfree((char*)ptr);
}

void operator delete[](void* ptr) {
    kfree((char*)ptr);
}

void memcpy(void* dst, void* src, size_t len) {
    char *d = (char*)dst;
    char *s = (char*)src;
    for(size_t i=0;i<len;i++)
        d[i] = s[i];
}

void memset(void* dst, char byte, size_t len) {
    char *d = (char*)dst;
    for(size_t i=0;i<len;i++)
        d[i] = byte;
}

void memclr(void* dst, size_t len) {
    char *d = (char*)dst;
    for(size_t i=0;i<len;i++)
        d[i] = 0;
}

int pow(int base, int exp) {
    int ret = 1;
    for(int i=0;i<exp;i++)
        ret *= base;
    return ret;
}

uint64_t floor(double n) {
    uint64_t int_part = (uint64_t)n;
    return int_part;
}

uint64_t ceil(double n) {
    return floor(n)+1;
}

double fractional(double n) {
    return n - floor(n);
}