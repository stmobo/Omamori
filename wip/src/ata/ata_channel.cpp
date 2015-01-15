/*
 * ata_channel.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "device/ata/ata_channel.h"

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
    this->delayed_starter->state = process_state::runnable;
    process_add_to_runqueue(this->delayed_starter);
}

void ata::ata_channel::transfer_cycle() {
    if( ata_controller.ready ) {
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
		message out("transfer_complete", this->current_transfer, sizeof(ata_transfer_request));
		this->current_transfer->requesting_process->send_message( out );
		process_add_to_runqueue( this->current_transfer->requesting_process );
		this->current_transfer = NULL;

		this>delayed_starter->state = process_state::runnable; // indirectly schedule ourselves to run later
		process_add_to_runqueue(this->delayed_starter);

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
