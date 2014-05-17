// print.h -- printing functions and stuff related to them
#pragma once

extern char* int_to_decimal(long long int);
extern char* double_to_decimal(double);
extern char* int_to_octal(long long int);
extern char* int_to_hex(long long int);
extern char* int_to_bin(long long int);
extern char* ksprintf_varg(const char*, ...);
extern void kprintf_varg(const char*, ...);
extern char* ksprintf(const char*, ...);
extern void kprintf(const char*, ...);
extern void panic(char*, ...);