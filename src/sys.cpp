// sys.cpp - miscellaneous system functions

#include "includes.h"
#include "sys.h"
#include "vga.h"
#include "dynmem.h"

const char* hex_alpha = "0123456789ABCDEF";
const char* dec_alpha = "0123456789";

void *__stack_chk_guard = NULL;

size_t strlen(char* str)
{
	size_t ret = 0;
	while ( str[ret] != 0 )
		ret++;
	return ret;
}

// byte/short/int/ll_to_hex - convert from integer to fixed-width string
// These functions convert from an integral type to a string.
// The caller is responsible for ensuring that *ret has enough space for the returned string!
char* byte_to_hex(char n, char *ret) {
    ret[0] = hex_alpha[ (n & 0x0F) ];
    ret[1] = hex_alpha[ (n >> 8) ];
    return ret;
}

char* short_to_hex(short n, char *ret) {
    ret[0] = hex_alpha[ ((n & 0xF000) >> 12) & 0xF ];
    ret[1] = hex_alpha[ (n & 0x0F00) >> 8 ];
    ret[2] = hex_alpha[ (n & 0x00F0) >> 4 ];
    ret[3] = hex_alpha[ (n & 0x000F) ];
    return ret;
}

char* int_to_hex(int n, char *ret) {
    ret[0] = hex_alpha[ ((n & 0xF0000000) >> 28) & 0xF ];
    ret[1] = hex_alpha[ (n & 0x0F000000) >> 24 ];
    ret[2] = hex_alpha[ (n & 0x00F00000) >> 20 ];
    ret[3] = hex_alpha[ (n & 0x000F0000) >> 16 ];
    ret[4] = hex_alpha[ (n & 0x0000F000) >> 12 ];
    ret[5] = hex_alpha[ (n & 0x00000F00) >> 8 ];
    ret[6] = hex_alpha[ (n & 0x000000F0) >> 4 ];
    ret[7] = hex_alpha[ (n & 0x0000000F) ];
    return ret;
}

// there is DEFINITELY a better way to do this.
char* int_to_decimal(int n) {
    int d = 0;
    int n2 = n;
    if(n == 0) {
        char* mem = kmalloc(2);
        mem[0] = '0';
        mem[1] = '\0';
    }
    while(n2 > 0) {
        d++;
        n2 /= 10;
    }
    n2 = n;
    d++;
    char* mem = kmalloc(d);
    for(int i=1;i<d;i++) {
        mem[d-(i+1)] = dec_alpha[(n2%10)];
        n2 /= 10;
    }
    mem[d-1] = '\0';
    return mem;
}

char* ll_to_hex(long long int n, char *ret) {
    for(int i=0;i<16;i++) {
        ret[(15-i)] = hex_alpha[(n>>(i*4))&0xF];
    }
    return ret;
}

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

void memcpy(char* dst, char* src, size_t len) {
    for(int i=0;i<len;i++)
        dst[i] = src[i];
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