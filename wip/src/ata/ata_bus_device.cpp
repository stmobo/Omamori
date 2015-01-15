/*
 * ata_bus_device.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "arch/x86/sys.h"
#include "device/ata/ata_bus_device.h"
#include "device/ata/ata_channel.h"

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

void ata::ata_device::do_pio_sector_transfer( void *buffer, bool write ) {
	uint16_t* current = (uint16_t*)buffer;
	while( ((io_inb( this->control_port ) & 0x80) > 0) || ((io_inb( this->control_port ) & 0x8) == 0) ); // busy-wait on DRQ
	for(unsigned int i=0;i<256;i++) {
		if( write )
			io_outw( this->base_port, *current++ );
		else
			*current++ = io_inw( this->base_port );
	}
}

void ata::ata_device::identify() {
	this->ident = kmalloc(512);

	kassert( (this->ident != NULL), "Could not allocate space for IDENTIFY command!\n" );

	this->channel->select( 0xA0 + ( this->is_slave ? 0x10 : 0x00 ) );
	io_outb( this->base_port+2, 0 );
	io_outb( this->base_port+3, 0 );
	io_outb( this->base_port+4, 0 );
	io_outb( this->base_port+5, 0 );
	io_outb( this->base_port+7, ATA_CMD_IDENTIFY );

	this->present = false;

	uint8_t stat = io_inb( this->control_port );
	if( stat > 0 ) {
		// okay, there actually IS something here
		if( !( (io_inb(this->base_port + 7) & 1) || ( (io_inb(this->base_port+4) == 0x14) && (io_inb(this->base_port+5) == 0xEB) ) ) ) { // is this an ATAPI / SATA device?
			// if not, continue..
			this->is_atapi = false;

			while( (io_inb( this->control_port ) & 0x80) > 0 ); // wait for BSY to clear
			if( !((io_inb( this->base_port+4 ) > 0) || (io_inb( this->base_port+5 ) > 0)) ) { // check for non-spec ATAPI devices
				while( (io_inb( this->control_port ) & 0x09) == 0 ); // wait for either DRQ or ERR to set
				if( (io_inb( this->control_port ) & 0x01) == 0 ) { // is ERR clear?
					this->do_pio_sector_transfer( this->ident, false ); // read 256 words of PIO data
					this->present = true;
					kprintf("Found ATA device on %s channel.", this->is_slave ? "slave" : "master");
				}
			}
		} else {
			this->is_atapi = true;

			io_outb( this->base_port+2, 0 );
			io_outb( this->base_port+3, 0 );
			io_outb( this->base_port+4, 0 );
			io_outb( this->base_port+5, 0 );
			io_outb( this->base_port+7, ATA_CMD_IDENTIFY_PACKET );

			while( (io_inb( this->control_port ) & 0x80) > 0 ); // wait for BSY to clear
			while( (io_inb( this->control_port ) & 0x09) == 0 ); // wait for either DRQ or ERR to set
			if( (io_inb( this->control_port ) & 0x01) == 0 ) { // is ERR clear?
				this->do_pio_sector_transfer( this->ident, false ); // read 256 words of PIO data
				this->present = true;
				kprintf("Found ATAPI device on %s channel.", this->is_slave ? "slave" : "master");
			}
		}
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
	uint16_t* current = (uint16_t*)req->buffer.remap();
	//kprintf("ata: buffer at %#p physical, %#p virtual.\n", req->buffer.buffer_phys, (void*)current);
	//kprintf("ata: PTE for virt address is %#x\n", paging_get_pte((size_t)current));
	for( unsigned int i=0;i<req->n_sectors;i++ ) {
		while( ((io_inb( this->control_port ) & ATA_SR_BSY) > 0) || ((io_inb( this->control_port ) & ATA_SR_DRQ) == 0) ) asm volatile("pause");
		for(unsigned int j=0;j<256;j++) {
			if( req->read )
				*current++ = io_inw( this->base_port );
			else
				io_outw( this->base_port, *current++ );
		}
		for(int k=0;k<4;k++) // 400 ns delay
			io_inb( this->control_port );
	}
	if( !req->read ) {
		if(req->to_slave) {
			this->channel->select( 0x10 );
		} else {
			this->channel->select( 0 );
		}
		while( ((io_inb( this->control_port ) & ATA_SR_BSY) > 0) || ((io_inb( this->control_port ) & ATA_SR_DRDY) == 0) ) asm volatile("pause");
		if( this->lba48 ) {
			io_outb( this->base_port+7, ATA_CMD_CACHE_FLUSH_EXT );
		} else {
			io_outb( this->base_port+7, ATA_CMD_CACHE_FLUSH );
		}
		while( ((io_inb( this->control_port ) & ATA_SR_BSY) > 0) ) asm volatile("pause");
	}
	flushCache(); // flush the (memory) cache cpu-side as well
	//kprintf("ata: PIO transfer complete.\n");
	k_vmem_free( (size_t)current );
}

void ata::ata_device::do_atapi_transfer( ata_transfer_request* req ) {
	// only one ATAPI transfer may be in progress per controller
	this->channel->controller->atapi_transfer_lock.lock();

	if(req->to_slave)
		this->channel->select( (1<<4) );
	else
		this->channel->select( 0 );

	// send ATA PACKET
	io_outb( this->base_port+1, 0 ); // Features port
	io_outb( this->base_port+2, 0 );
	io_outb( this->base_port+3, 0 );
	io_outb( this->base_port+4, 0 ); // LBA-Mid
	io_outb( this->base_port+5, 8 ); // LBA-Hi
	io_outb( this->base_port+7, ATA_CMD_PACKET );

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

	uint8_t *cmd_ptr = cmd;

	while( ((io_inb( this->control_port ) & ATA_SR_BSY) > 0) || ((io_inb( this->control_port ) & ATA_SR_DRQ) == 0) );
	for(unsigned int j=0;j<6;j++) {
		io_outw( this->base_port, (uint16_t)*(cmd_ptr++) );
	}

	for(int k=0;k<4;k++) // 400 ns delay
		io_inb( this->control_port );

	uint8_t lba_mid = io_inb( this->base_port+4 );
	uint8_t lba_hi = io_inb( this->base_port+5 );

	uint16_t packet_sz = (((uint16_t)lba_hi) << 8) | lba_mid;

	void* data = req->buffer.remap();
	uint16_t *current = (uint16_t*)data;

	while( ((io_inb( this->control_port ) & ATA_SR_BSY) > 0) || ((io_inb( this->control_port ) & ATA_SR_DRQ) == 0) ) asm volatile("pause");
	for(unsigned int j=0;j<packet_sz/2;j++) {
		if( req->read )
			*current++ = io_inw( this->base_port );
		else
			io_outw( this->base_port, *current++ );
	}
	for(int k=0;k<4;k++) // 400 ns delay
		io_inb( this->control_port );

	this->channel->controller->atapi_transfer_lock.unlock();
}
