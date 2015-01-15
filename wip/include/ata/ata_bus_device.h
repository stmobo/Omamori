/*
 * ata_bus_device.h
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "../../../../wip/include/ata/ata_channel.h"

namespace ata {

	struct ata_device {
		bool  present;
		bool  is_slave;
		void* ident;

		uint64_t n_sectors;
		bool  lba48;
		bool  is_atapi;
		char  model[41];
		char  serial[21];

		unsigned int base_port;
		unsigned int control_port;
		unsigned int channel_number;
		bool idle;

		ata_channel *channel;

		void initialize();
		void identify();
		void do_pio_sector_transfer( void *buffer, bool write );
		void do_pio_transfer( ata_transfer_request* req );
		void do_ata_transfer( ata_transfer_request* req );
		void do_atapi_transfer( ata_transfer_request* req );
		ata_device( unsigned int ch_no, bool slave, unsigned int base, unsigned int ctrl ) { this->is_slave = slave; this->base_port = base; this->control_port = ctrl; this->channel_number = ch_no; };
	};

};
