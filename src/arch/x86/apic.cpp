/*
 * apic.cpp
 *
 *  Created on: Jan 27, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "arch/x86/sys.h"
#include "arch/x86/apic.h"
#include "arch/x86/pic.h"
#include "core/paging.h"
#include "core/acpi.h"
#include "lib/vector.h"

uint64_t lapic_base;
uintptr_t lapic_vaddr;
bool lapic_initialized;

typedef struct io_apic {
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
} io_apic;

typedef struct local_apic {
	uint8_t processor_id;
	uint8_t lapic_id;
	unsigned int nmi_pin;
	bool nmi_polarity;
} local_apic;

vector< io_apic* > io_apics;
vector< local_apic* > local_apics;

bool lapic_detect() {
	uint64_t lapic_msr = read_msr( 0x0000001B );
	if( lapic_msr & (1<<11) ) { // there is a lapic here
		lapic_base = lapic_msr & 0xfffff000;
		//lapic_base = ( lapic_msr >> 12 ) & 0xFFFFFFFFFFFFF;
		kprintf("LAPIC detected, base at %#llx\n", lapic_base);
		return true;
	} else {
		kprintf("No LAPIC detected.\n");
		return false;
	}
}

void lapic_setbase( uintptr_t addr ) {
	write_msr( 0x1B, (addr & 0xFFFFF000) | 0x800 );
	lapic_base = addr;
}

uintptr_t lapic_getbase() {
	uint64_t lapic_msr = read_msr( 0x0000001B );
	lapic_base = lapic_msr & 0xfffff000;
	return lapic_msr & 0xfffff000;
	//return ( lapic_msr >> 12 ) & 0xFFFFFFFF;
}

inline uint32_t lapic_read_register( uint32_t addr ) {
	uint32_t *lapic_mem = (uint32_t*)(lapic_vaddr+addr);
	return *lapic_mem;
}

inline void lapic_write_register( uint32_t addr, uint32_t val ) {
	uint32_t *lapic_mem = (uint32_t*)(lapic_vaddr+addr);
	*lapic_mem = val;
}

void lapic_initialize() {
	// force interrupts to APIC (IMCR)
	io_outb( 0x22, 0x70 );
	io_outb( 0x23, 1 );

	lapic_vaddr = k_vmem_alloc(1);
	paging_set_pte( lapic_vaddr, lapic_base, (1<<6) );

	uint32_t lapic_ver_reg = lapic_read_register( 0x30 );
	unsigned char lapic_version = lapic_ver_reg & 0xFF;

	uint32_t lapic_id_reg = lapic_read_register( 0x20 );
	unsigned char lapic_id = (lapic_id_reg>>24) & 0xFF;

	// Parse ACPI MADT table, find NMI
	char table_sig[4] = { 'A', 'P', 'I', 'C' };
	ACPI_TABLE_HEADER *madt_base;
	ACPI_STATUS stat = AcpiGetTable( table_sig, 1, &madt_base );

	if( stat != AE_OK ) {
		kprintf("apic: failed to retrieve MADT, error code 0x%x: %s\n", stat, const_cast<char*>(AcpiFormatException(stat)));
		return;
	}

	unsigned int nmi_pin = 1;
	uint16_t nmi_flags = 0;

	uint8_t *madt_entries = (uint8_t*)(madt_base+0x29);
	while( ((uintptr_t)madt_entries) < (((uintptr_t)madt_base)+(madt_base->Length)) ) {
		uint8_t len = madt_entries[1];
		uint8_t type = madt_entries[0];

		if( type == 4 ) {
			uint8_t id = madt_entries[2];
			if( id == lapic_id || id == 0xFF ) {
				nmi_pin = madt_entries[5];
				nmi_flags = *((uint16_t*)((uintptr_t)madt_entries)+3);
				break;
			}
		}

		madt_entries = (uint8_t*)( ((uintptr_t)madt_entries)+len );
	}

	uint8_t nmi_polarity = nmi_flags & 3;

	uint32_t nmi_lint_entry = (4<<8);
	if( nmi_polarity == 3 ) {
		// active low
		nmi_lint_entry |= (1<<14);
	}

	if( nmi_pin == 0 ) {
		lapic_write_register( 0x350, nmi_lint_entry );
	} else if( nmi_pin == 1 ) {
		lapic_write_register( 0x360, nmi_lint_entry );
	}

	lapic_write_register( 0xF0, 0x1FF );

	lapic_initialized = true;
	kprintf("apic: Initialized version %#x LAPIC with ID = %#x and NMI pin %u (%s).\n", lapic_version, lapic_id, nmi_pin, (nmi_polarity == 3) ? "active low" : "active high");
}

void lapic_eoi() {
	lapic_write_register( 0xB0, 1 );
}

void enumerate_apics() {
	char table_sig[4] = { 'A', 'P', 'I', 'C' };
	ACPI_TABLE_HEADER *madt_base;
	ACPI_STATUS stat = AcpiGetTable( table_sig, 1, &madt_base );

	if( stat != AE_OK ) {
		kprintf("apic: failed to retrieve MADT, error code 0x%x: %s\n", stat, const_cast<char*>(AcpiFormatException(stat)));
		return;
	} else {
		kprintf("Scanning MADT (length %#x)\n", madt_base->Length);
	}

	uint8_t *madt_entries = (uint8_t*)(((uintptr_t)madt_base)+44);
	while( ((uintptr_t)madt_entries) < (((uintptr_t)madt_base)+(madt_base->Length)) ) {
		uint8_t len = madt_entries[1];
		uint8_t type = madt_entries[0];
		kprintf("Found MADT entry of type %u (length %u).\n", type, len);
		logger_flush_buffer();

		switch(type) {
		case 0: // LAPIC entry
		{
			local_apic *o = new local_apic;
			o->lapic_id = madt_entries[2];
			o->processor_id = madt_entries[3];
			local_apics.add_end(o);
			kprintf("Found LAPIC entry for LAPIC ID %#x.\n", o->lapic_id);
			break;
		}
		case 6: // IO SAPIC entry
		{
			bool ioapic_overridden = false;
			for(unsigned int i=0;i<io_apics.count();i++) {
				if( io_apics[i]->ioapic_id == madt_entries[2] ) {
					io_apics[i]->paddr = *(uint32_t*)(((uintptr_t)madt_entries)+4);
					io_apics[i]->int_base = *(uint32_t*)(((uintptr_t)madt_entries)+8);
					ioapic_overridden = true;
					kprintf("Found overriding IO-SAPIC entry for ID %#x.\n", madt_entries[2]);
					break;
				}
			}
			if( ioapic_overridden ) {
				break;
			} // else fall through
		}
		case 1: // IOAPIC entry
		{
			io_apic *o = new io_apic;
			o->ioapic_id = madt_entries[2];
			o->paddr = *(uint32_t*)(((uintptr_t)madt_entries)+4);
			o->int_base = *(uint32_t*)(((uintptr_t)madt_entries)+8);
			io_apics.add_end(o);
			kprintf("Found IOAPIC entry for IOAPIC ID %#x (GSI base = %u).\n", o->ioapic_id, o->int_base);
			break;
		}
		case 4: // NMI Pin entry
		{
			for(unsigned int i=0;i<local_apics.count();i++) {
				if( (local_apics[i]->lapic_id == madt_entries[2]) || (madt_entries[2] == 0xFF) ) {
					local_apics[i]->nmi_pin = madt_entries[5];
					local_apics[i]->nmi_polarity = ((madt_entries[3]&3) == 3);
				}
			}
			kprintf("Found LAPIC NMI entry for LAPIC ID %#x.\n", madt_entries[2]);
		}
		break;
		default:
		break;
		}

		madt_entries = (uint8_t*)( ((uintptr_t)madt_entries)+len );
	}
}

uint32_t io_apic::read_register( uint32_t index ) {
	*(uint32_t*)(this->vaddr) = index;
	return *(uint32_t*)(this->vaddr+0x10);
}

void io_apic::write_register( uint32_t index, uint32_t value ) {
	*(uint32_t*)(this->vaddr) = index;
	*(uint32_t*)(this->vaddr+0x10) = value;
}

void io_apic::set_redir_entry( ioapic_redir_entry *ent, unsigned int index ) {
	if( this->max_redir_entry > index ) {
		uint32_t reg1 = ent->vector;
		reg1 |= ( ent->delivery_mode << 8 );
		reg1 |= ( ( ent->logical_destination ? 1 : 0 ) << 11 );
		reg1 |= ( ( ent->active_low ? 1 : 0 ) << 13 );
		reg1 |= ( ( ent->remote_irr ? 1 : 0 ) << 14 );
		reg1 |= ( ( ent->level_triggered ? 1 : 0 ) << 15 );
		reg1 |= ( ( ent->masked ? 1 : 0 ) << 16 );
		this->write_register( 0x10+(index*2), reg1 );
		this->write_register( 0x11+(index*2), (ent->destination << 24) );
	}
}

void io_apic::update_redir_entries() {
	for(unsigned int i=0;i<this->max_redir_entry;i++) {
		this->set_redir_entry( this->entries+i, i );
	}
}

void io_apic::initialize() {
	this->vaddr = k_vmem_alloc(1);
	paging_set_pte( this->vaddr, this->paddr, (1<<6) );

	uint32_t io_apic_ver = this->read_register( 1 );

	this->max_redir_entry = (io_apic_ver >> 16) & 0xFF;
	this->entries = new ioapic_redir_entry[this->max_redir_entry];

	for(unsigned int i=0;i<this->max_redir_entry;i++) {
		this->entries[i].vector = 0xFE;
		this->entries[i].destination = local_apics[0]->lapic_id;
	}

	this->update_redir_entries();

	kprintf("apic: Initialized IOAPIC with ID %#x at p%#x / v%#x and %u interrupts from %u.\n", this->ioapic_id, this->paddr, this->vaddr, this->max_redir_entry+1, this->int_base);
}

void initialize_apics() {
	if( lapic_detect() ) {
		if( pic_8259_initialized ) {
			// disable the 8259
			pic_set_mask(0xFFFF);
			pic_8259_initialized = false;
		}

		enumerate_apics();

		for(unsigned int i=0;i<io_apics.count();i++) {
			io_apics[i]->initialize();
		}

		lapic_initialize();

		// now identity-map the original 8259 interrupts
		// first, find the IO APIC handing interrupts from base 0.
		io_apic* isa_apic = NULL;
		for(unsigned int i=0;i<io_apics.count();i++) {
			if( io_apics[i]->int_base == 0 ) {
				isa_apic = io_apics[i];
				break;
			}
		}

		if( isa_apic != NULL ) {
			if( isa_apic->max_redir_entry >= 16 ) {
				for(unsigned int i=0;i<16;i++) {
					isa_apic->entries[i].vector = 32+i;
					isa_apic->entries[i].level_triggered = false;
					isa_apic->entries[i].active_low = false;
					isa_apic->entries[i].destination = local_apics[0]->lapic_id;
				}
			}


			// now handle MADT Interrupt Source Overrides
			char table_sig[4] = { 'A', 'P', 'I', 'C' };
			ACPI_TABLE_HEADER *madt_base;
			ACPI_STATUS stat = AcpiGetTable( table_sig, 1, &madt_base );
			if( stat != AE_OK ) {
				kprintf("apic: failed to retrieve MADT, error code 0x%x: %s\n", stat, const_cast<char*>(AcpiFormatException(stat)));
				return;
			} else {
				kprintf("Scanning MADT for Interrupt Source Overrides\n");
			}

			uint8_t *madt_entries = (uint8_t*)(((uintptr_t)madt_base)+44);
			while( ((uintptr_t)madt_entries) < (((uintptr_t)madt_base)+(madt_base->Length)) ) {
				uint8_t len = madt_entries[1];
				uint8_t type = madt_entries[0];

				if( type == 2 ) {
					uint32_t redirect_from = *(uint32_t*)( ((uintptr_t)madt_entries)+4 );
					uint8_t redirect_to = madt_entries[3];
					isa_apic->entries[redirect_from].vector = 32+redirect_to;
					kprintf("apic: IOAPIC ISA interrupt %u redirected to IRQ%u.\n", redirect_from, redirect_to);
				}

				madt_entries = (uint8_t*)( ((uintptr_t)madt_entries)+len );
			}

			isa_apic->update_redir_entries();
		}
	}
}

void apic_set_gsi_vector( unsigned int gsi, ioapic_redir_entry ent ) {
	for(unsigned int i=0;i<io_apics.count();i++) {
		if( (io_apics[i]->int_base <= gsi) && ( (io_apics[i]->int_base + io_apics[i]->max_redir_entry) >= gsi ) ) {
			io_apics[i]->entries[gsi - io_apics[i]->int_base] = ent;
			io_apics[i]->update_redir_entries();
			return;
		}
	}
}
