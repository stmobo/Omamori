// print.h -- printing functions and stuff related to them
#pragma once
#include <stdarg.h>
//#define ENABLE_SERIAL_LOGGING

extern char* int_to_decimal(long long int);
extern char* double_to_decimal(double);
extern char* int_to_octal(long long int);
extern char* int_to_hex(long long int);
extern char* int_to_bin(long long int);
extern char* concatentate_strings(char*, char*);
extern char* append_char(char*, char);
extern char* ksprintf_varg(const char*, va_list);
extern void  kprintf_varg(const char*, va_list);
extern char* ksprintf(const char*, ...);
extern void  kprintf(const char*, ...);
extern void  panic(char*, ...);