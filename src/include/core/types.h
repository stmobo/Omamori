/*
 * types.h
 *
 *  Created on: Feb 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"

#ifdef __x86__
#include "arch/x86/types.h"
#endif

typedef bool(*irq_handler)(irq_num_t);
