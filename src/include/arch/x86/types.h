/*
 * types.h
 *
 *  Created on: Feb 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"

// handy types for working with addresses and the like (for more portability)
// though for pointers you really should be working with uintptr_t's instead

typedef uint32_t phys_addr_t;
typedef uint32_t virt_addr_t;

typedef uint32_t phys_diff_t;
typedef uint32_t virt_diff_t;

typedef uint8_t  irq_num_t;

typedef bool interrupt_status_t;
