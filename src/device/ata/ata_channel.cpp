/*
 * ata_channel.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "arch/x86/sys.h"
#include "device/ata.h"
#include "core/message.h"

void ata::ata_channel::select( uint8_t select_val ) {
	if( this->selected_drive != select_val ) {
		this->selected_drive = select_val;
		io_outb( this->base+6, select_val );
		// read status 4 times (giving it time to switch)
		for(int i=0;i<4;i++)
			io_inb( this->control );
	}
}

void ata::ata_channel::enqueue_request( ata_transfer_request* req ) {
    if( req->read ) {
        this->read_queue.add_end( req );
    } else {
        this->write_queue.add_end( req );
    }

    if( this->currently_idle ) {
        this->current_operation = req->read;
        this->currently_idle = false;
        //this->transfer_cycle();
    }
    process_wake(this->delayed_starter);
}

void ata::ata_channel::transfer_cycle() {
    if( this->controller->ready ) {
        this->current_operation = !this->current_operation;
        if( (this->current_operation ? this->write_queue : this->read_queue).count() == 0 ) {
            this->current_operation = !this->current_operation;
            if( (this->current_operation ? this->write_queue : this->read_queue).count() == 0 ) {
                this->current_transfer = NULL;
                this->currently_idle = true;
                return;
            }
        }

        this->current_transfer = (this->current_operation ? this->write_queue : this->read_queue).remove();

        if(this->current_transfer->to_slave) {
        	if(this->slave != NULL) {
        		this->slave->do_pio_transfer( this->current_transfer );
        	}
        } else {
        	if(this->master != NULL) {
				this->master->do_pio_transfer( this->current_transfer );
			}
        }

        this->current_transfer->status = true;
        //kprintf("ata_channel: transfer complete (id=%llu)\n", this->current_transfer->id);
		message out(this->current_transfer, 0);
		send_to_channel("transfer_complete", out);
		this->current_transfer = NULL;

		//this>delayed_starter->state = process_state::runnable; // indirectly schedule ourselves to run later
		//process_add_to_runqueue(this->delayed_starter);

        this->currently_idle = false;
    }
}

bool ata::ata_channel::transfer_available() {
    bool op = this->current_operation;
    if( (op ? this->write_queue : this->read_queue).count() == 0 ) {
        op = !op;
        if( (op ? this->write_queue : this->read_queue).count() == 0 ) {
            return false;
        }
    }
    return true;
}

void ata::ata_channel::perform_requests( ata_channel *ch ) {
	while(true) {
		if( ch->transfer_available() ) {
			ch->delayed_starter->state = process_state::runnable;
			ch->transfer_cycle();
		} else {
			process_sleep();
		}
	}
}

void ata::ata_channel::irq() {
	if( this->waiting_on_atapi_irq ) {
		this->waiting_on_atapi_irq = false;
	}
	io_inb( this->base+7 ); // clear IRQ flag
	process_wake( this->delayed_starter );
}


ata::ata_channel::ata_channel( ata_controller* controller, unsigned int channel_no, short base, short control ) {
	this->controller = controller;
	this->channel_no = channel_no;
	this->base = base;
	this->control = control;

	// reset control register (both devices)
	this->select( 0x40 );
	io_outb( control, 0 );
	this->select( 0x50 );
	io_outb( control, 0 );

	this->master = new ata_device( this, false );
	this->slave = new ata_device( this, true );

	if(this->master->present) {
		ata_io_disk *disk = new ata_io_disk( this, this->master );

		device_manager::device_node* dev = new device_manager::device_node;
		device_manager::device_node* base = &device_manager::root; //controller->dev_node;
		dev->child_id = base->children.count();
		dev->enabled = true;
		dev->type = device_manager::dev_type::storage_controller;
		if( !this->master->is_atapi ) {
			dev->human_name = const_cast<char*>("ATA Master Drive");
		} else {
			dev->human_name = const_cast<char*>("ATAPI Master Drive");
		}
		dev->device_data = (void*)disk;

		base->children.add_end(dev);
		this->master->dev = dev;

		io_register_disk( disk );
	}

	if(this->slave->present) {
		ata_io_disk *disk = new ata_io_disk( this, this->slave );

		device_manager::device_node* dev = new device_manager::device_node;
		device_manager::device_node* base = &device_manager::root; //this->controller->dev_node;
		dev->child_id = base->children.count();
		dev->enabled = true;
		dev->type = device_manager::dev_type::storage_controller;
		if( !this->slave->is_atapi ) {
			dev->human_name = const_cast<char*>("ATA Slave Drive");
		} else {
			dev->human_name = const_cast<char*>("ATAPI Slave Drive");
		}
		dev->device_data = (void*)disk;

		base->children.add_end(dev);
		this->slave->dev = dev;

		io_register_disk( disk );
	}

	void(*ptr)(ata_channel*) = (void(*)(ata_channel*))&this->perform_requests;
	this->delayed_starter = new process(reinterpret_cast<uint32_t>(ptr), false, 0, "ata_channel_request_server", this, 1);

	kprintf("    * master: %s\n", this->master->model);
	kprintf("    * %s, %u sectors addressable\n", ( this->master->lba48 ? "LBA48 supported" : "LBA48 not supported" ), this->master->n_sectors);
	kprintf("    * slave: %s\n", this->slave->model);
	kprintf("    * %s, %u sectors addressable\n", ( this->slave->lba48 ? "LBA48 supported" : "LBA48 not supported" ), this->slave->n_sectors);
}

void ata::ata_io_disk::send_request( transfer_request* req ) {
	ata_transfer_request *ata_req = new ata_transfer_request(*req);

	ata_req->to_slave = this->device->is_slave;
	this->channel->enqueue_request(ata_req);
}

unsigned int ata::ata_io_disk::get_sector_size()  { return this->device->sector_size; };
unsigned int ata::ata_io_disk::get_total_size()  { return this->device->n_sectors * this->device->sector_size; };
