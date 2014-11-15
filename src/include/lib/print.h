// print.h -- printing functions and stuff related to them
#pragma once
#include <stdarg.h>
#define ENABLE_SERIAL_LOGGING

#ifndef __PRINTF_NO_UTILITY
extern char* int_to_decimal(unsigned long long int);
extern char* double_to_decimal(double);
extern char* int_to_octal(unsigned long long int);
extern char* int_to_hex(unsigned long long int);
extern char* int_to_bin(unsigned long long int);
extern char* append_char(char*, char);
#endif
extern char* concatentate_strings(char*, char*);
extern void  reverse(char*);
extern int   get_n_digits(int num, int base=10);
extern signed int atoi( char* );
extern char* itoa( signed long long int num, int base=10, bool add_space=false, bool add_plus=false, bool add_prefix=false );
extern char* dtoa( double num, int precision=6, int base=10, bool add_space=false, bool add_plus=false );
extern char* dtosn( double num, int precision=18, int base=10, bool add_space=false, bool add_plus=false );
extern int   kvsnprintf(char*, size_t, const char*, va_list);
extern int   kvsprintf(char*, const char*, va_list);
extern int   kvprintf(const char*, va_list);
extern int   ksnprintf(char*, size_t, const char*, ...);
extern int   ksprintf(char*, const char*, ...);
extern int   kprintf(const char*, ...);
extern void  panic(char*, ...);

#define      __kassert_stringifier_2(x) #x
#define      __kassert_stringifier_1(x) __kassert_stringifier_2(x)
#define      kassert( exp, message ) do { if( !(exp) ) { panic( __FILE__ " ( " __kassert_stringifier_1(__LINE__) " ): " message ); } } while(0)

extern void  logger_initialize();
extern void  logger_flush_buffer();