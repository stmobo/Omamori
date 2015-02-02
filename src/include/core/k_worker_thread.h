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
	typedef unsigned int(*work_func)(void*, unsigned int);

	struct work {
		uint32_t spawning_pid;
		void* context_1;
		unsigned int context_2;
		work_func func;

		unsigned int return_code;
		bool finished;
		bool auto_remove;
		channel_receiver* ch;

		uint64_t work_id;

		unsigned int wait();
		work( work_func func, void* context_1, unsigned int context_2, bool auto_remove );
		~work() { delete this->ch; };
	};

	work* schedule( work_func func, void* context_1 = NULL, unsigned int context_2 = 0, bool auto_remove=true );
	void reap_orphans();
	void start();
};
