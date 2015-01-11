// debug.h

#pragma once
#include "includes.h"

extern uint32_t *get_current_ebp();
extern uint32_t *get_caller_ebp( uint32_t* );
extern void stack_trace_walk(unsigned int);
