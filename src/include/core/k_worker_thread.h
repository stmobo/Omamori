/*
 * k_worker_thread.h
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/scheduler.h"

namespace k_work {
	struct work {
		uint32_t spawning_pid;
		unsigned int(*func)(void);

		unsigned int return_code;
		bool finished;
		bool auto_remove;

		uint64_t work_id;

		unsigned int wait();
		work( unsigned int(*func)(void), bool auto_remove );
	};

	work* schedule( unsigned int(*func)(void), bool auto_remove=true );
	void reap_orphans();
	void start();
};
