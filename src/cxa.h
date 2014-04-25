#pragma once
#include "includes.h"
#define ATEXIT_MAX_FUNCS            256

struct cxa_term_func {
    void(*func)(void*);
    void *parameter;
    void *homeDSO;
    cxa_term_func();
    cxa_term_func(const cxa_term_func*);
};

typedef struct cxa_term_func cxa_termination_func;

extern "C" {
extern int __cxa_atexit(void(*)(void*), void*, void*);
extern void __cxa_finalize(void*);
}
// extern void atexit(void(*)(void*), void*) -- this is in sys.h