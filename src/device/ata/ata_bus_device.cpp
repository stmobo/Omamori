/*
 * ata_bus_device.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "arch/x86/sys.h"
#include "device/ata.h"

void ata::ata_device::do_pio_sector_transfer( void *buffer, bool write ) {
	uint16_t* current = (uint16_t*)buffer;
	while( ((io_inb( this->channel->control ) & 0x80) > 0) || ((io_inb( this->channel->control ) & 0x8) == 0) ); // busy-wait on DRQ
	for(unsigned int i=0;i<256;i++) {
		if( write )
			io_outw( this->channel->base, *current++ );
		else
			*current++ = io_inw( this->channel->base );
	}
}

void ata::ata_device::identify() {
	this->ident = kmalloc(512);

	kassert( (this->ident != NULL), "Could not allocate space for IDENTIFY command!\n" );

	this->channel->select( 0xA0 + ( this->is_slave ? 0x10 : 0x00 ) );
	io_outb( this->channel->base+2, 0 );
	io_outb( this->channel->base+3, 0 );
	io_outb( this->channel->base+4, 0 );
	io_outb( this->channel->base+5, 0 );
	io_outb( this->channel->base+7, ATA_CMD_IDENTIFY );

	this->present = false;

	uint8_t stat = io_inb( this->channel->control );
	if( stat > 0 ) {
		// okay, there actually IS something here
		if( !( (io_inb(this->channel->base + 7) & 1) || ( (io_inb(this->channel->base+4) == 0x14) && (io_inb(this->channel->base+5) == 0xEB) ) ) ) { // is this an ATAPI / SATA device?
			// if not, continue..
			this->is_atapi = false;

			while( (io_inb( this->channel->control ) & 0x80) > 0 ); // wait for BSY to clear
			if( !((io_inb( this->channel->base+4 ) > 0) || (io_inb( this->channel->base+5 ) > 0)) ) { // check for non-spec ATAPI devices
				while( (io_inb( this->channel->control ) & 0x09) == 0 ); // wait for either DRQ or ERR to set
				if( (io_inb( this->channel->control ) & 0x01) == 0 ) { // is ERR clear?
					this->do_pio_sector_transfer( this->ident, false ); // read 256 words of PIO data
					this->present = true;
					kprintf("Found ATA device on %s channel.\n", this->is_slave ? "slave" : "master");
				}
			}
		} else {
			this->is_atapi = true;

			io_outb( this->channel->base+2, 0 );
			io_outb( this->channel->base+3, 0 );
			io_outb( this->channel->base+4, 0 );
			io_outb( this->channel->base+5, 0 );
			io_outb( this->channel->base+7, ATA_CMD_IDENTIFY_PACKET );

			while( (io_inb( this->channel->control ) & 0x80) > 0 ); // wait for BSY to clear
			while( (io_inb( this->channel->control ) & 0x09) == 0 ); // wait for either DRQ or ERR to set
			if( (io_inb( this->channel->control ) & 0x01) == 0 ) { // is ERR clear?
				this->do_pio_sector_transfer( this->ident, false ); // read 256 words of PIO data
				this->present = true;
				kprintf("Found ATAPI device on %s channel.\n", this->is_slave ? "slave" : "master");
			}
		}
	}
}

ata::ata_device::ata_device( ata_channel* channel, bool slave ) {
	this->channel = channel;
	this->is_slave = slave;

	this->identify();

	this->lba48 = ( (((uint16_t*)this->ident)[83] & 0x400) );

	for(unsigned int i=0;i<40;i+=2) {
		this->model[i+1] = ((char*)this->ident)[54+i];
		this->model[i] = ((char*)this->ident)[55+i];
	}

	this->model[40] = 0;

	for(unsigned int i=0;i<20;i+=2) {
		this->serial[i+1] =  ((char*)this->ident)[20+i];
		this->serial[i] =  ((char*)this->ident)[21+i];
	}

	this->serial[20] = 0;

	if(!this->is_atapi) {
		if( this->lba48 ) {
			this->n_sectors = ((uint64_t*)this->ident)[25];
		} else {
			this->n_sectors = ((uint32_t*)this->ident)[30];
		}
	} else {
		uint8_t cmd[12] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

		this->send_atapi_command(cmd);

		uint8_t lba_mid = io_inb( this->channel->base+4 );
		uint8_t lba_hi = io_inb( this->channel->base+5 );

		uint16_t packet_sz = (((uint16_t)lba_hi) << 8) | lba_mid;

		void* data = kmalloc(12);
		uint16_t *current = (uint16_t*)data;
		uint8_t *current_bytes = (uint8_t*)current;

		while( ((io_inb( this->channel->control ) & ATA_SR_BSY) > 0) || ((io_inb( this->channel->control ) & ATA_SR_DRQ) == 0) ) asm volatile("pause");
		for(unsigned int j=0;j<packet_sz/2;j++) {
			*current++ = io_inw( this->channel->base );
		}
		for(int k=0;k<4;k++) // 400 ns delay
			io_inb( this->channel->control );

		// wait for another IRQ
		this->channel->waiting_on_atapi_irq = true;
		process_sleep();

		uint32_t last_lba = (((uint32_t)current_bytes[0])<<24) | (((uint32_t)current_bytes[1])<<16) | (((uint32_t)current_bytes[2])<<8) | ((uint32_t)current_bytes[3]);
		uint32_t block_size = (((uint32_t)current_bytes[4])<<24) | (((uint32_t)current_bytes[5])<<16) | (((uint32_t)current_bytes[6])<<8) | ((uint32_t)current_bytes[7]);

		this->n_sectors = last_lba;

		kprintf("ata: ATAPI device has %u sectors with a block size of %u.\n", last_lba, block_size);
	}
}

void ata::ata_device::do_pio_transfer( ata_transfer_request* req ) {
	if( !this->is_atapi ) {
		return this->do_ata_transfer( req );
	} else {
		return this->do_atapi_transfer( req );
	}
}

void ata::ata_device::do_ata_transfer( ata_transfer_request* req ) {
	if( this->lba48 ) {
		if(this->is_slave)
			this->channel->select( 0xF0 );
		else
			this->channel->select( 0xE0 );
		//kprintf("ata: sending transfer parameters.\n * n_sectors = %u\n * sector_start(LBA48) = %u\n", req->n_sectors, req->sector_start);
		io_outb( this->channel->base+2, (req->n_sectors>>8)    & 0xFF );
		io_outb( this->channel->base+3, (req->sector_start>>24)& 0xFF );
		io_outb( this->channel->base+4, (req->sector_start>>32)& 0xFF );
		io_outb( this->channel->base+5, (req->sector_start>>40)& 0xFF );

		io_outb( this->channel->base+2, req->n_sectors         & 0xFF );
		io_outb( this->channel->base+3, req->sector_start      & 0xFF );
		io_outb( this->channel->base+4, (req->sector_start>>8) & 0xFF );
		io_outb( this->channel->base+5, (req->sector_start>>16)& 0xFF );
	} else {
		if(req->to_slave)
			this->channel->select( 0xF0 | ( (req->sector_start >> 24) & 0x0F ) );
		else
			this->channel->select( 0xE0 | ( (req->sector_start >> 24) & 0x0F ) );
		//kprintf("ata: sending transfer parameters.\n * n_sectors = %u\n * sector_start(LBA28) = %u\n", req->n_sectors, req->sector_start);
		io_outb( this->channel->base+2, req->n_sectors         & 0xFF );
		io_outb( this->channel->base+3, req->sector_start      & 0xFF );
		io_outb( this->channel->base+4, (req->sector_start>>8) & 0xFF );
		io_outb( this->channel->base+5, (req->sector_start>>16)& 0xFF );
	}

	while( ((io_inb( this->channel->control ) & ATA_SR_DRDY) == 0) ) asm volatile("pause");
	if(this->lba48) {
		if( req->read ) {
			//kprintf("ata: sending ATA_CMD_READ_PIO_EXT.\n");
			io_outb( this->channel->base+7, ATA_CMD_READ_PIO_EXT );
		} else {
			//kprintf("ata: sending ATA_CMD_WRITE_PIO_EXT.\n");
			io_outb( this->channel->base+7, ATA_CMD_WRITE_PIO_EXT );
		}
	} else {
		if( req->read ) {
			//kprintf("ata: sending ATA_CMD_READ_PIO.\n");
			io_outb( this->channel->base+7, ATA_CMD_READ_PIO );
		} else {
			//kprintf("ata: sending ATA_CMD_WRITE_PIO.\n");
			io_outb( this->channel->base+7, ATA_CMD_WRITE_PIO );
		}
	}

	uint16_t* current = (uint16_t*)req->buffer.remap();
	//kprintf("ata: buffer at %#p physical, %#p virtual.\n", req->buffer.buffer_phys, (void*)current);
	//kprintf("ata: PTE for virt address is %#x\n", paging_get_pte((size_t)current));
	for( unsigned int i=0;i<req->n_sectors;i++ ) {
		while( ((io_inb( this->channel->control ) & ATA_SR_BSY) > 0) || ((io_inb( this->channel->control ) & ATA_SR_DRQ) == 0) ) asm volatile("pause");
		for(unsigned int j=0;j<256;j++) {
			if( req->read )
				*current++ = io_inw( this->channel->base );
			else
				io_outw( this->channel->base, *current++ );
		}
		for(int k=0;k<4;k++) // 400 ns delay
			io_inb( this->channel->control );
	}
	if( !req->read ) {
		if(req->to_slave) {
			this->channel->select( 0x10 );
		} else {
			this->channel->select( 0 );
		}
		while( ((io_inb( this->channel->control ) & ATA_SR_BSY) > 0) || ((io_inb( this->channel->control ) & ATA_SR_DRDY) == 0) ) asm volatile("pause");
		if( this->lba48 ) {
			io_outb( this->channel->base+7, ATA_CMD_CACHE_FLUSH_EXT );
		} else {
			io_outb( this->channel->base+7, ATA_CMD_CACHE_FLUSH );
		}
		while( ((io_inb( this->channel->control ) & ATA_SR_BSY) > 0) ) asm volatile("pause");
	}
	flushCache(); // flush the (memory) cache cpu-side as well
	//kprintf("ata: PIO transfer complete.\n");
	k_vmem_free( (size_t)current );
}

void ata::ata_device::send_atapi_command(uint8_t *command_bytes) {
	uint16_t* cmd = (uint16_t*)command_bytes;

	if(this->is_slave)
		this->channel->select( (1<<4) );
	else
		this->channel->select( 0 );

	// send ATA PACKET
	io_outb( this->channel->base+1, 0 ); // Features port
	io_outb( this->channel->base+2, 0 );
	io_outb( this->channel->base+3, 0 );
	io_outb( this->channel->base+4, 0 ); // LBA-Mid
	io_outb( this->channel->base+5, 8 ); // LBA-Hi
	io_outb( this->channel->base+7, ATA_CMD_PACKET );

	while( ((io_inb( this->channel->control ) & ATA_SR_BSY) > 0) || ((io_inb( this->channel->control ) & ATA_SR_DRQ) == 0) );
	for(unsigned int j=0;j<6;j++) {
		io_outw( this->channel->base, (uint16_t)*(cmd++) );
	}

	for(int k=0;k<4;k++) // 400 ns delay
		io_inb( this->channel->control );

	this->channel->waiting_on_atapi_irq = true;
	process_sleep();
}

void ata::ata_device::do_atapi_transfer( ata_transfer_request* req ) {
	// only one ATAPI transfer may be in progress per controller
	this->channel->controller->atapi_transfer_lock.lock();

	uint8_t cmd[12];

	if(req->read) {
		cmd[0] = 0xA8;
	} else {
		cmd[0] = 0xAA;
	}

	// 6 - 9 : transfer size
	// 2 - 5 : transfer start address

	cmd[2] = (req->sector_start >> 24) & 0xFF; // MSB
	cmd[3] = (req->sector_start >> 16) & 0xFF;
	cmd[4] = (req->sector_start >> 8) & 0xFF;
	cmd[5] = req->sector_start & 0xFF; // LSB

	cmd[6] = (req->n_sectors >> 24) & 0xFF;
	cmd[7] = (req->n_sectors >> 16) & 0xFF;
	cmd[8] = (req->n_sectors >> 8) & 0xFF;
	cmd[9] = req->n_sectors & 0xFF;

	this->send_atapi_command(cmd);

	uint8_t lba_mid = io_inb( this->channel->base+4 );
	uint8_t lba_hi = io_inb( this->channel->base+5 );

	uint16_t packet_sz = (((uint16_t)lba_hi) << 8) | lba_mid;

	void* data = req->buffer.remap();
	uint16_t *current = (uint16_t*)data;

	while( ((io_inb( this->channel->control ) & ATA_SR_BSY) > 0) || ((io_inb( this->channel->control ) & ATA_SR_DRQ) == 0) ) asm volatile("pause");
	for(unsigned int j=0;j<packet_sz/2;j++) {
		if( req->read )
			*current++ = io_inw( this->channel->base );
		else
			io_outw( this->channel->base, *current++ );
	}
	for(int k=0;k<4;k++) // 400 ns delay
		io_inb( this->channel->control );

	// wait for another IRQ
	this->channel->waiting_on_atapi_irq = true;
	process_sleep();

	this->channel->controller->atapi_transfer_lock.unlock();
}
