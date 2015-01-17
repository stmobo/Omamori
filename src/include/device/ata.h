/*
 * ata.h
 *
 *  Created on: Jan 15, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/io.h"
#include "core/scheduler.h"
#include "device/pci.h"

#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

#define ATA_ER_BBK      0x80
#define ATA_ER_UNC      0x40
#define ATA_ER_MC       0x20
#define ATA_ER_IDNF     0x10
#define ATA_ER_MCR      0x08
#define ATA_ER_ABRT     0x04
#define ATA_ER_TK0NF    0x02
#define ATA_ER_AMNF     0x01

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC
#define ATAPI_CMD_READ            0xA8
#define ATAPI_CMD_EJECT           0x1B

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA        0x00
#define IDE_ATAPI      0x01

#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07

#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

// Channels:
#define      ATA_PRIMARY      0x00
#define      ATA_SECONDARY    0x01

// Directions:
#define      ATA_READ      0x00
#define      ATA_WRITE     0x01

#define      ATA_BUS_MASTER_START       0x550

namespace ata {
	struct ata_channel;
	struct ata_controller;
	struct ata_device;

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

		static bool handle_irq();
		static bool handle_irq14();
		static bool handle_irq15();
		ata_controller( pci_device *dev );
	};

	struct ata_transfer_request : public transfer_request {
		bool to_slave;

		ata_transfer_request( transfer_request& cpy ) : transfer_request(cpy) {};
		ata_transfer_request( ata_transfer_request& cpy ) : transfer_request(cpy), to_slave(cpy.to_slave) {};
	};

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
		bool 				  waiting_on_atapi_irq = false;

		ata_controller*       controller;

		ata_device* master;
		ata_device* slave;

		void select( uint8_t select_val );
		void enqueue_request( ata_transfer_request* );
		void transfer_cycle();
		bool transfer_available();

		void irq();
		static void perform_requests( ata_channel* ch );

		ata_channel( ata_controller* controller, unsigned int channel_no, short base, short control );
	};

	struct ata_device {
		bool  present;
		bool  is_slave;
		void* ident;

		uint64_t n_sectors;
		bool  lba48;
		bool  is_atapi;
		char  model[41];
		char  serial[21];
		bool idle;

		ata_channel *channel;

		void initialize();
		void identify();
		void do_pio_sector_transfer( void *buffer, bool write );
		void do_pio_transfer( ata_transfer_request* req );
		void do_ata_transfer( ata_transfer_request* req );
		void do_atapi_transfer( ata_transfer_request* req );
		void send_atapi_command(uint8_t *command_bytes);
		ata_device( ata_channel* channel, bool slave );
	};


	struct ata_io_disk : public io_disk {
		ata_channel *channel;
		ata_device* device;

		void send_request( transfer_request* );
		unsigned int  get_sector_size() { if( this->device->is_atapi ) return 2048; return 512; };
		unsigned int  get_total_size();

		ata_io_disk( ata_channel* ch, ata_device *dev ) { this->channel = ch; this->device = dev; };
	};

	void initialize();
	extern ata_controller* controller;
};
