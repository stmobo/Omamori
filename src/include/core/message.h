/*
 * message.h
 *
 *  Created on: Jan 24, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/scheduler.h"
#include "lib/vector.h"
#include "lib/sync.h"
#include <stdarg.h>

struct message;

typedef struct channel {
	vector< process_ptr > listeners;
	vector< message* > message_queue;
	uint64_t current_uid;
	mutex    lock;

	channel() : current_uid(0) {};
	void send( message& msg );
} channel;

typedef struct message {
	void *data;
	size_t data_size;
	process_ptr sender;
	uint64_t uid;
	uint32_t n_receivers;

	~message() { if( (this->data != NULL) && (this->data_size > 0) ) { kfree(this->data); } };

	message( message& rhs );
	message( message* rhs);
	message();
	message(void *data, size_t data_sz );
} message;

typedef class channel_receiver {
	channel *remote_channel;
	uint64_t last_seen_uid;
	void sort_internal( unsigned int lo_index, unsigned int hi_index );
public:
	vector< message* > queue;

	channel_receiver& operator=( channel_receiver& rhs );

	void wait();
	bool update();
	void sort();
	channel_receiver( channel* remote );
	channel_receiver( const channel_receiver& copy );
	~channel_receiver();
} channel_receiver;

channel_receiver listen_to_channel( char* channel_name );
void send_to_channel( char* channel_name, message& msg );
void register_channel( char* channel_name );
unsigned int wait_multiple( unsigned int n_receivers, channel_receiver* recv_1, ... );
