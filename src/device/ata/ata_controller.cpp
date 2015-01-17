/*
 * ata_controller.cpp
 *
 *  Created on: Jan 15, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "device/ata.h"
#include "arch/x86/sys.h"
#include "arch/x86/irq.h"

ata::ata_controller* ata::controller;

ata::ata_controller::ata_controller( pci_device *dev ) {
	this->device = dev;

	this->primary_channel   = 0x1F0; //this->device->registers[0].raw;
	this->primary_control   = 0x3F6; //this->device->registers[1].raw;
	this->secondary_channel = 0x170; //this->device->registers[2].raw;
	this->secondary_control = 0x376; //this->device->registers[3].raw;
	this->dma_control_base  = this->device->registers[4].raw;

	/*
	kprintf("ata: primary channel at %s address %#x.\n", (this->device->registers[0].io_space ? "IO" : "Memory"), this->device->registers[0].raw );
	kprintf("ata: primary control at %s address %#x.\n", (this->device->registers[1].io_space ? "IO" : "Memory"), this->device->registers[1].raw );
	kprintf("ata: secondary channel at %s address %#x.\n", (this->device->registers[2].io_space ? "IO" : "Memory"), this->device->registers[2].raw );
	kprintf("ata: secondary control at %s address %#x.\n", (this->device->registers[3].io_space ? "IO" : "Memory"), this->device->registers[3].raw );
	*/
	kprintf("ata: controller BMIDE registers at %s address %#x.\n", (this->device->registers[4].io_space ? "IO" : "Memory"), this->device->registers[4].raw);
	// PCI command register:
	pci_write_config_16( this->device->bus, this->device->device, this->device->func, 0x04, 0x5 ); // Bus Master Enable | I/O Space Enable

	// determine IRQs:
	uint8_t prior_config = pci_read_config_8( this->device->bus, this->device->device, this->device->func, 0x3C );
	uint8_t int_pin = pci_read_config_8( this->device->bus, this->device->device, this->device->func, 0x3D );
	pci_write_config_8( this->device->bus, this->device->device, this->device->func, 0x3C, 0xFE );
	if( pci_read_config_8( this->device->bus, this->device->device, this->device->func, 0x3C ) == 0xFE ) {
		// device needs IRQ assignment (we'll just use IRQ14 in this case)
		kprintf("ata: controller IRQ assignment required (previous assignment was %u, interrupt pin is %u)\n", prior_config, int_pin);
		irq_add_handler(14, (size_t)(&this->handle_irq));
		pci_write_config_8( this->device->bus, this->device->device, this->device->func, 0x3C, 14 );
		if( pci_read_config_8( this->device->bus, this->device->device, this->device->func, 0x3C ) != 14 ) {
			// write didn't go through?
			kprintf("ata: controller IRQ assignment didn't go through?\n");
		}
	} else {
		// device doesn't need an IRQ assignment, is it PATA?
		if( (this->device->prog_if == 0x8A) || (this->device->prog_if == 0x80) ) {
			// it's PATA, so we just assign IRQs 14 and 15.
			kprintf("ata: controller is PATA, no IRQ assignment required\n");
			irq_add_handler(14, (size_t)(&this->handle_irq14));
			irq_add_handler(15, (size_t)(&this->handle_irq15));
		} else {
			// in this case we must be using the APIC (Interrupt Line field is R/O)
			// well, just do nothing in this case (we don't support the APIC yet)
			kprintf("ata: Interrupt Line field is for APIC\n");
		}
	}

	uint8_t test_ch0 = io_inb(this->primary_channel+7);
	uint8_t test_ch1 = io_inb(this->secondary_channel+7);

	if( test_ch0 != 0xFF ) {
		//ata_channels[0].delayed_starter = new process( (size_t)&irq14_delayed_starter, 0, false, "ata_ch0_delayedstarter", NULL, 0 );
		//spawn_process( ata_channels[0].delayed_starter );

		this->channels[0] = new ata_channel( this, 0, this->primary_channel, this->primary_control );
		kprintf("ata: devices connected on channel 0 (not floating).\n");
	} else {
		// floating bus, there's nothing here
		kprintf("ata: no devices connected on channel 0.\n");
	}

	if( test_ch1 != 0xFF ) {
		//ata_channels[1].delayed_starter = new process( (size_t)&irq15_delayed_starter, 0, false, "ata_ch1_delayedstarter", NULL, 0 );
		//spawn_process( ata_channels[1].delayed_starter );

		this->channels[1] = new ata_channel( this, 1, this->secondary_channel, this->secondary_control );
		kprintf("ata: devices connected on channel 1 (not floating).\n");
	} else {
		kprintf("ata: no devices connected on channel 1.\n");
	}

	this->ready = true;
}

bool ata::ata_controller::handle_irq() {
	kprintf("ata: IRQ received.\n");
	if(controller->channels[0] != NULL) {
		controller->channels[0]->irq();
	}
	if(controller->channels[1] != NULL) {
		controller->channels[1]->irq();
	}
	return true;
}

bool ata::ata_controller::handle_irq14() {
	if(controller->channels[0] != NULL) {
		controller->channels[0]->irq();
	}
	return true;
}

bool ata::ata_controller::handle_irq15() {
	if(controller->channels[1] != NULL) {
		controller->channels[1]->irq();
	}
	return true;
}

void ata::initialize() {
	for(unsigned int i=0;i<pci_devices.count();i++) {
		pci_device *current = pci_devices[i];
		if( (current->class_code == 0x01) && (current->subclass_code == 0x01) ) {
			kprintf("ata: found controller (device ID: %u [%u/%u/%u])\n", i, current->bus, current->device, current->func);
			controller = new ata_controller( current );
			break;
		}
	}
}