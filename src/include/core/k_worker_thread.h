/*
 * k_worker_thread.h
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/scheduler.h"
#include "core/message.h"

namespace k_work {
	struct work {
		uint32_t spawning_pid;
		void* context_1;
		unsigned int context_2;
		unsigned int(*func)(void*, unsigned int);

		unsigned int return_code;
		bool finished;
		bool auto_remove;
		channel_receiver* ch;

		uint64_t work_id;

		unsigned int wait();
		work( unsigned int(*func)(void*, unsigned int), void* context_1, unsigned int context_2, bool auto_remove );
		~work() { delete this->ch; };
	};

	work* schedule( unsigned int(*func)(void*, unsigned int), void* context_1 = NULL, unsigned int context_2 = 0, bool auto_remove=true );
	void reap_orphans();
	void start();
};
