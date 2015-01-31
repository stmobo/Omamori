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
#include "device/pit.h"

uint64_t lapic_base;
uintptr_t lapic_vaddr;
bool apics_initialized;

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

uint32_t lapic_read_register( uint32_t addr ) {
	volatile uint32_t *lapic_mem = (uint32_t*)(lapic_vaddr+addr);
	return *lapic_mem;
}

void lapic_write_register( uint32_t addr, uint32_t val ) {
	volatile uint32_t *lapic_mem = (uint32_t*)(lapic_vaddr+addr);
	*lapic_mem = val;
}

void lapic_initialize() {
	//system_disable_interrupts();
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
		//system_enable_interrupts();
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

	uint32_t extint_lint_entry = (7<<8);

	if( nmi_pin == 0 ) {
		lapic_write_register( 0x350, nmi_lint_entry );
		lapic_write_register( 0x360, extint_lint_entry );
	} else if( nmi_pin == 1 ) {
		lapic_write_register( 0x350, extint_lint_entry );
		lapic_write_register( 0x360, nmi_lint_entry );
	}

	lapic_write_register( 0xD0, 0x01000000 );
	lapic_write_register( 0xE0, 0xFFFFFFFF );
	lapic_write_register( 0x80, 0 );

	//system_enable_interrupts();

	kprintf("apic: enabling local APIC.\n");
	logger_flush_buffer();
	lapic_write_register( 0xF0, 0x1FF );

	apics_initialized = true;

	// self-test IPI
	kprintf("apic: self-test IPI 1.\n");
	logger_flush_buffer();
	lapic_write_register( 0x300, 33 | (1<<14) | (1<<18) );
	kprintf("apic: self-test IPI 2.\n");
	logger_flush_buffer();
	lapic_write_register( 0x300, 33 | (1<<14) | (1<<18) );

	// set up timer:
	lapic_write_register( 0x3E0, 3 ); // set timer divide to 16
	lapic_write_register( 0x320, 32 ); // enable APIC timer (mapped to IRQ0)

	// set PIT channel 2 for the same frequency
	io_outb( 0x61, io_inb(0x61)&0xFD ); // disable PC Speaker
	io_outb( 0x43, 0xB2 ); // PIT channel 2, lo-hi sequential access, hardware retriggerable one-shot
	io_outb( 0x42, 0x9B ); // LSB
	io_inb(0x60);
	io_outb( 0x42, 0x2E ); // MSB

	uint8_t tmp = io_inb( 0x61 ) & 0xFE;
	io_outb(  0x61, tmp );
	io_outb(  0x61, tmp | 1 );
	lapic_write_register( 0x380, 0xFFFFFFFF ); // start counting

	while( !( (io_inb(0x61) & 0x20) > 0) );

	lapic_write_register( 0x320, 0x10000 ); // stop APIC timer
	uint32_t current_apic_tmr_val = lapic_read_register( 0x390 );
	uint32_t interval = 0xFFFFFFFF - current_apic_tmr_val;
	interval += 1;
	uint32_t cpu_bus_freq = interval * 16 * 100;
	uint32_t apic_timer_interval = interval / 1000;
	apic_timer_interval /= 16;

	lapic_write_register( 0x2F0, 0x10000 ); // CMCI
	lapic_write_register( 0x330, 0x10000 ); // Thermal
	lapic_write_register( 0x340, 0x10000 ); // Perf. Counters
	lapic_write_register( 0x370, 0x10000 ); // Error
	lapic_write_register( 0x320, 32 | (1<<17) ); // set up timer periodic interrupt
	lapic_write_register( 0x3E0, 3 );
	lapic_write_register( 0x380, (apic_timer_interval < 16 ? 16 : apic_timer_interval) ); // set up timer again

	kprintf("apic: interval set to %u.\n", (apic_timer_interval < 16 ? 16 : apic_timer_interval) );

	io_outb( 0x43, (3<<4) ); // disable PIT
	io_outb( 0x40, 0 ); // as best we can, anyways

	// force interrupts to APIC (IMCR)
	io_outb( 0x22, 0x70 );
	io_outb( 0x23, 1 );

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
	volatile uint32_t* base = (uint32_t*)this->vaddr;
	*(base) = index;
	return base[4];
}

void io_apic::write_register( uint32_t index, uint32_t value ) {
	volatile uint32_t* base = (uint32_t*)this->vaddr;
	*(base) = index;
	base[4] = value;
	//*(uint32_t*)(this->vaddr+0x10) = value;
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
		this->write_register( 0x11+(index*2), (((uint32_t)ent->destination) << 24) );
	}
}

void io_apic::update_redir_entries() {
	for(unsigned int i=0;i<this->max_redir_entry;i++) {
		uint32_t reg1 = this->entries[i].vector;
		reg1 |= ( this->entries[i].delivery_mode << 8 );
		reg1 |= ( ( this->entries[i].logical_destination ? 1 : 0 ) << 11 );
		reg1 |= ( ( this->entries[i].active_low ? 1 : 0 ) << 13 );
		reg1 |= ( ( this->entries[i].remote_irr ? 1 : 0 ) << 14 );
		reg1 |= ( ( this->entries[i].level_triggered ? 1 : 0 ) << 15 );
		reg1 |= ( ( this->entries[i].masked ? 1 : 0 ) << 16 );
		this->write_register( 0x10+(i*2), reg1 );
		this->write_register( 0x11+(i*2), (((uint32_t)this->entries[i].destination) << 24) );
	}
}

void io_apic::initialize() {
	this->vaddr = k_vmem_alloc(1);
	paging_set_pte( this->vaddr, this->paddr, (1<<6) );

	uint32_t io_apic_ver = this->read_register( 1 );

	this->max_redir_entry = (io_apic_ver >> 16) & 0xFF;
	this->entries = new ioapic_redir_entry[this->max_redir_entry];

	for(unsigned int i=0;i<this->max_redir_entry;i++) {
		this->entries[i].delivery_mode = ioapic_delivery_mode::fixed;
		this->entries[i].active_low = false;
		this->entries[i].level_triggered = false;
		this->entries[i].masked = false;
		this->entries[i].remote_irr = false;
		this->entries[i].logical_destination = false;

		this->entries[i].vector = 32 + this->int_base + i;
		this->entries[i].destination = local_apics[0]->lapic_id;
	}

	this->update_redir_entries();

	kprintf("apic: Initialized IOAPIC with ID %#x at p%#x / v%#x and %u interrupts from %u.\n", this->ioapic_id, this->paddr, this->vaddr, this->max_redir_entry+1, this->int_base);
}

void initialize_apics() {
	if( lapic_detect() ) {
		enumerate_apics();

		lapic_initialize();

		if( pic_8259_initialized ) {
			// disable the 8259
			pic_set_mask(0xFFFF);
			pic_8259_initialized = false;
		}

		for(unsigned int i=0;i<io_apics.count();i++) {
			io_apics[i]->initialize();
		}

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
			/*
			if( isa_apic->max_redir_entry >= 16 ) {
				for(unsigned int i=0;i<16;i++) {
					isa_apic->entries[i].vector = 32+i;
					isa_apic->entries[i].level_triggered = false;
					isa_apic->entries[i].active_low = false;
					isa_apic->entries[i].destination = local_apics[0]->lapic_id;
				}
				kprintf("apic: assigned ISA IRQs to IO APIC %u.\n", isa_apic->ioapic_id);
			}
			*/


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
					uint8_t flags = madt_entries[8];
					if( (flags & 3) == 3 ) {
						isa_apic->entries[redirect_from].active_low = true;
						kprintf("apic: IOAPIC ISA interrupt %u is active low.\n", redirect_from);
					}
					if( ((flags >> 2) & 3) == 3 ) {
						isa_apic->entries[redirect_from].level_triggered = true;
						kprintf("apic: IOAPIC ISA interrupt %u is level triggered.\n", redirect_from);
					}
					isa_apic->entries[redirect_from].vector = 32+redirect_to;
					kprintf("apic: IOAPIC ISA interrupt %u redirected to IRQ%u.\n", redirect_from, redirect_to);
				}

				madt_entries = (uint8_t*)( ((uintptr_t)madt_entries)+len );
			}
			isa_apic->update_redir_entries();
		}

		apics_initialized = true;
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

ioapic_redir_entry apic_get_gsi_vector( unsigned int gsi ) {
	for(unsigned int i=0;i<io_apics.count();i++) {
		if( (io_apics[i]->int_base <= gsi) && ( (io_apics[i]->int_base + io_apics[i]->max_redir_entry) >= gsi ) ) {
			return io_apics[i]->entries[gsi - io_apics[i]->int_base];
		}
	}

	ioapic_redir_entry emp;
	return emp;
}
