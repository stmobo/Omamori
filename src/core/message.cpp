/*
 * message_mockup.cpp
 *
 *  Created on: Jan 23, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "core/message.h"
#include "lib/hash_table.h"

hash_table< channel* > channels(0x1000);

message::message() {
	process_ptr p( process_current );
	this->sender = p;
	this->uid = 0;
	this->n_receivers = 0;
	this->data = NULL;
	this->data_size = 0;
}

message::message( void* data, size_t data_sz ) {
	process_ptr p( process_current );
	this->sender = p;
	this->uid = 0;
	this->n_receivers = 0;
	this->data = data;
	this->data_size = data_sz;
}

message::message( message& rhs ) {
	this->n_receivers = rhs.n_receivers;
	this->uid = rhs.uid;

	this->sender = rhs.sender;

	if( (rhs.data != NULL) && (rhs.data_size > 0) ) {
		this->data = kmalloc(rhs.data_size);
		this->data_size = rhs.data_size;
		memcpy( this->data, rhs.data, rhs.data_size );
	} else {
		this->data = rhs.data;
		this->data_size = rhs.data_size;
	}
}

message::message( message* rhs ) {
	this->n_receivers = rhs->n_receivers;
	this->uid = rhs->uid;

	this->sender = rhs->sender;

	if( (rhs->data != NULL) && (rhs->data_size > 0) ) {
		this->data = kmalloc(rhs->data_size);
		this->data_size = rhs->data_size;
		memcpy( this->data, rhs->data, rhs->data_size );
	} else {
		this->data = rhs->data;
		this->data_size = rhs->data_size;
	}
}

channel_receiver::channel_receiver( channel* remote ) {
	this->remote_channel = remote;
	if( remote == NULL )
		return;

	this->remote_channel->lock.lock();

	this->last_seen_uid = remote->current_uid;

	bool already_there = false;
	for(unsigned int i=0;i<this->remote_channel->listeners.count();i++) {
		if( this->remote_channel->listeners[i].raw_ptr() == process_current ) {
			already_there = true;
			break;
		}
	}
	if( !already_there ) {
		process_ptr p(process_current);
		this->remote_channel->listeners.add_end( p );
	}

	this->remote_channel->lock.unlock();
}

channel_receiver::channel_receiver( const channel_receiver& copy ) {
	this->remote_channel = copy.remote_channel;
	this->last_seen_uid = copy.last_seen_uid;
	this->queue = copy.queue;
}

channel_receiver& channel_receiver::operator=( channel_receiver& rhs ) {
	this->remote_channel = rhs.remote_channel;
	this->last_seen_uid = rhs.last_seen_uid;
	this->queue = rhs.queue;

	return *this;
}

channel_receiver::~channel_receiver() {
	this->remote_channel->lock.lock();

	for(unsigned int i=0;i<this->remote_channel->listeners.count();i++) {
		if( this->remote_channel->listeners[i].raw_ptr() == process_current ) {
			this->remote_channel->listeners.remove(i);
			break;
		}
	}

	for(unsigned int i=0;i<this->queue.count();i++) {
		delete this->queue[i];
	}

	this->remote_channel->lock.unlock();
}

bool channel_receiver::update() {
	this->remote_channel->lock.lock();

	if( this->remote_channel->message_queue.count() > 0 ) {
		if( this->remote_channel->message_queue[0]->uid >= this->last_seen_uid ) {
			unsigned int first_new = 0;
			for(unsigned int i=1;i<this->remote_channel->message_queue.count();i++) {
				if( this->remote_channel->message_queue[i]->uid <= this->last_seen_uid ) {
					first_new = i-1;
					break;
				}
			}

			this->last_seen_uid = this->remote_channel->message_queue[0]->uid;
			vector< unsigned int > to_delete;

			for(unsigned int i=0;i<=first_new;i++) {
				message *msg = new message( this->remote_channel->message_queue[i] );
				this->remote_channel->message_queue[i]->n_receivers--;
				if( this->remote_channel->message_queue[i]->n_receivers == 0 ) {
					to_delete.add_end(i);
				}
				this->queue.add_end(msg);
			}

			unsigned int offset = 0;
			for(unsigned int i=0;i<to_delete.count();i++) {
				message *msg = this->remote_channel->message_queue.remove(to_delete[i-offset]);
				delete msg;
				offset++;
			}

			this->remote_channel->lock.unlock();
			return true;
		}
	}

	this->remote_channel->lock.unlock();
	return false;
}

void channel_receiver::wait() {
	while(true) {
		if( this->update() ) {
			//kprintf("messaging: update got something\n");
			break;
		}
		if( this->queue.count() > 0 ) {
			//kprintf("messaging: there's still something left\n");
			break;
		}
		//kprintf("messaging: nothing here, going to sleep now\n");
		process_sleep();
	}
}

void channel::send( message& msg ) {
	if( this->listeners.count() == 0 ) {
		return;
	}

	this->lock.lock();

	msg.uid = this->current_uid++;
	msg.n_receivers = this->listeners.count();

	message *m = new message(msg);

	this->message_queue.add( m );

	for(unsigned int i=0;i<this->listeners.count();i++) {
		process_wake(this->listeners[i]);
	}

	this->lock.unlock();
}

channel_receiver listen_to_channel( char* channel_name ) {
	channel* ch = channels[ channel_name ];

	if(ch == NULL) {
		kprintf("messaging: Process %u attempted to listen to a nonexistent channel %s.\n", process_current->id, channel_name);
	}

	channel_receiver recv(ch);
	return recv;
}

void send_to_channel( char* channel_name, message& msg ) {
	channel* ch = channels[ channel_name ];

	if(ch == NULL) {
		kprintf("messaging: Process %u attempted to send to a nonexistent channel %s.\n", process_current->id, channel_name);
		return;
	}

	ch->send( msg );
}

void register_channel( char* channel_name ) {
	channel *ch = new channel;
	channels.set(channel_name, ch);
}

unsigned int wait_multiple( unsigned int n_receivers, channel_receiver* recv_1, ... ) {
	vector< channel_receiver* > recv_list;
	va_list args;
	va_start(args, recv_1);

	recv_list.add_end(recv_1);

	for(unsigned int i=1;i<n_receivers;i++) {
		channel_receiver* t = va_arg(args, channel_receiver*);
		recv_list.add_end(t);
	}

	while(true) {
		for(unsigned int i=0;i<n_receivers;i++) {
			if( recv_list[i]->update() ) {
				va_end(args);
				return i;
			}

			if( recv_list[i]->queue.count() > 0 ) {
				va_end(args);
				return i;
			}
		}

		process_sleep();
	}

	return 0;
}
