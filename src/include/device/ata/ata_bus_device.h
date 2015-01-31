/*
 * ata_bus_device.h
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/device_manager.h"

namespace ata {

	struct ata_device {
		bool  present;
		bool  is_slave;
		void* ident;


		bool  lba48;
		bool  is_atapi;
		char  model[41];
		char  serial[21];
		bool idle;

		ata_channel *channel;

		uint64_t n_sectors;
		uint32_t sector_size;

		device_manager::device_node* dev;
		//ata_io_disk *io_disk;

		void initialize();
		void identify();
		void do_pio_sector_transfer( void *buffer, bool write );
		void do_pio_transfer( ata_transfer_request* req );
		void do_ata_transfer( ata_transfer_request* req );
		void do_atapi_transfer( ata_transfer_request* req );
		void send_atapi_command( uint8_t *cmd );
		ata_device( ata_channel* channel, bool slave );
	};

};
