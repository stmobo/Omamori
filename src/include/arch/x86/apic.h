/*
 * apic.h
 *
 *  Created on: Jan 28, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"

enum ioapic_delivery_mode {
	fixed = 0,
	lowest_priority = 1,
	smi = 2,
	nmi = 4,
	init = 5,
	extint = 7
};

typedef struct io_redir_table_entry {
	uint8_t vector;
	ioapic_delivery_mode delivery_mode;
	bool logical_destination;
	bool active_low;
	bool remote_irr;
	bool level_triggered;
	bool masked;
	uint8_t destination;
} ioapic_redir_entry;

extern uint64_t lapic_base;
bool lapic_detect();
void initialize_apics();
void lapic_eoi();
void apic_set_gsi_vector( unsigned int gsi, ioapic_redir_entry ent );
