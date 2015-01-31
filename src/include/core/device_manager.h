/*
 * device_manager.h
 *
 *  Created on: Jan 31, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "lib/vector.h"

namespace device_manager {

	typedef enum class device_node_type {
		computer,   		 // "root node" type thing
		processor, 		 	 // NUMA domain?
		lapic,				 // local APICs
		ioapic,			     // I/O APICs (8259s count too)
		timer,				 // PITs and the LAPIC Timer
		pci_device,			 // PCI devices
		pci_bus,			 // PCI busses
		usb_bus,			 // USB hub / bus things
		ps2_controller,		 // the 8042 controller
		storage_controller,  // The ATA controller, etc.
		storage_unit,		 // The actual HDDs / SSDs / CD Drives / Flash Drives / etc.
		human_input,		 // Mice, keyboards
		human_output,		 //	Monitors, serial-out
		unknown				 // everything else
	} dev_type;

	typedef enum class device_resource_type {
		memory,
		io_port,
		interrupt,
		unknown
	} res_type;

	typedef struct device_memory_resource {
		uintptr_t start;	   // Both
		uintptr_t end;		   // inclusive!
	} res_memory;

	typedef struct device_resource {
		res_type type;
		bool consumes; // true if consuming defined resource, false if providing defined resources
		union {
			res_memory memory;
			uint16_t io_port;
			uint8_t interrupt;
		};
	} device_resource;

	typedef struct device_tree_node {
		device_node_type type;
		bool enabled;
		void* device_data;
		char* human_name;
		vector< device_tree_node* > children;
		vector< device_resource* > resources;
		unsigned int child_id;
		unsigned int global_id;

		device_tree_node();
	} device_node;


	extern device_node root;

};
