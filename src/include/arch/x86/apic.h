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

struct io_apic {
	uint32_t int_base;
	uint8_t ioapic_id;
	uint8_t max_redir_entry;
	uintptr_t paddr;
	uintptr_t vaddr;

	ioapic_redir_entry *entries;

	void set_redir_entry( ioapic_redir_entry* ent, unsigned int index );
	void update_redir_entries();
	void initialize();
	uint32_t read_register( uint32_t index );
	void write_register( uint32_t index, uint32_t value );
};

struct local_apic {
	uint8_t processor_id;
	uint8_t lapic_id;
	unsigned int nmi_pin;
	bool nmi_polarity;
};

extern uint64_t lapic_base;
extern bool apics_initialized;
extern vector< io_apic* > io_apics;
extern vector< local_apic* > local_apics;

bool lapic_detect();
void initialize_apics();
void lapic_eoi();
void apic_set_gsi_vector( unsigned int gsi, ioapic_redir_entry ent );
ioapic_redir_entry apic_get_gsi_vector( unsigned int gsi );

inline uint32_t lapic_read_register( uint32_t addr );
inline void lapic_write_register( uint32_t addr, uint32_t val );
