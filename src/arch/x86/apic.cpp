/*
 * apic.cpp
 *
 *  Created on: Jan 27, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/apic.h"

uint64_t lapic_base;

void lapic_detect() {
	uint64_t lapic_msr = read_msr( 0x0000001B );
	if( lapic_msr & (1<<11) ) { // there is a lapic here
		lapic_base = ( lapic_msr >> 12 ) & 0xFFFFFFFFFFFFF;
		kprintf("LAPIC detected, base at %#llx\n", lapic_base);
	} else {
		kprintf("No LAPIC detected.\n");
	}
}
