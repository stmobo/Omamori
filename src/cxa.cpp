// cxa.cpp - __cxa_* functions

#include "cxa.h"

cxa_term_func::cxa_term_func() {
    this->func = NULL;
    this->parameter = NULL;
    this->homeDSO = NULL;
}

cxa_term_func::cxa_term_func(const cxa_term_func *obj2) {
    this->func = obj2->func;
    this->parameter = obj2->parameter;
    this->homeDSO = obj2->homeDSO;
}

cxa_termination_func __cxa_exit_funcs[ATEXIT_MAX_FUNCS];

extern "C" int __cxa_atexit( void (*destructor)(void*), void* arg, void *dso ) {
    for(int i=0;i<ATEXIT_MAX_FUNCS;i++) {
        if(__cxa_exit_funcs[i].func == NULL) {
            __cxa_exit_funcs[i].func = destructor;
            __cxa_exit_funcs[i].parameter = arg;
            __cxa_exit_funcs[i].homeDSO = dso;
            return 0;
        }
    }
    return 1;
}

void __cxa_atexit_list_shift() {
    for(int i=0;i<ATEXIT_MAX_FUNCS;i++) {
        if(__cxa_exit_funcs[i].func == NULL) {
            __cxa_exit_funcs[i] = __cxa_exit_funcs[i+1];
            __cxa_exit_funcs[i+1].func = NULL;
            __cxa_exit_funcs[i+1].parameter = NULL;
            __cxa_exit_funcs[i+1].homeDSO = NULL;
        }
    }
}

extern "C" void __cxa_finalize(void* dso_handle) {
    for(int i=0;i<ATEXIT_MAX_FUNCS;i++) {
        if(__cxa_exit_funcs[i].func != NULL) {
            if(__cxa_exit_funcs[i].homeDSO == dso_handle || dso_handle == NULL) {
                __cxa_exit_funcs[i].func(__cxa_exit_funcs[i].parameter);
                __cxa_exit_funcs[i].func = NULL;
                __cxa_exit_funcs[i].parameter = NULL;
                __cxa_exit_funcs[i].homeDSO = NULL;
            }
        }
    }
    __cxa_atexit_list_shift();
}

int atexit( void (*func)(void*), void* parameter ) {
    return __cxa_atexit( func, parameter, NULL );
}
