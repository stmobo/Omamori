/*
 * ata_controller.h
 *
 *  Created on: Jan 15, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "device/pci.h"
#include "core/device_manager.h"

namespace ata {
	struct ata_channel;

	struct ata_controller {
		pci_device *device;

		unsigned short primary_channel   = 0;
		unsigned short primary_control   = 0;
		unsigned short secondary_channel = 0;
		unsigned short secondary_control = 0;
		unsigned short dma_control_base  = 0;

		bool ready = false;
		mutex atapi_transfer_lock;
		bool atapi_data_ready = false;
		bool atapi_data_wait = false;

		ata_channel* channels[2];
		device_manager::device_node* dev_node;

		ata_controller( pci_device *dev );
		static bool handle_irq();
		static bool handle_irq14();
		static bool handle_irq15();
	};
};
