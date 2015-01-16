/*
 * ata_channel.h
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "device/pci.h"
#include "core/io.h"
#include "core/scheduler.h"

struct ata::ata_device;

namespace ata {

	struct ata_transfer_request : public transfer_request {
		bool to_slave;

		ata_transfer_request( transfer_request& cpy ) : transfer_request(cpy) {};
		ata_transfer_request( ata_transfer_request& cpy ) : transfer_request(cpy), to_slave(cpy.to_slave) {};
	};

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

	    void initialize();
	}

	struct ata_channel {
		unsigned int          channel_no;
		short                 base;
		short                 control;
		bool                  interrupt;
		bool                  currently_idle = true;
		uint8_t               selected_drive = 0;
		mutex                 lock;
		ata_transfer_request* current_transfer = NULL;
		vector<ata_transfer_request*> read_queue;
		vector<ata_transfer_request*> write_queue;
		bool                  current_operation = false; // false - read, true - write
		process*              delayed_starter;
		ata_controller*       controller;

		ata_device* master;
		ata_device* slave;

		void select( uint8_t select_val );
		void enqueue_request( ata_transfer_request* );
		void transfer_cycle();
		bool transfer_available();
	};

	struct ata_io_disk {
		bool slave_disk;
		ata_channel *channel;
		ata_device* device;

		void send_request( transfer_request* );
		unsigned int  get_sector_size() { return 512; };
		unsigned int  get_total_size();
	};

};
