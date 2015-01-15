/*
 * k_worker_thread.cpp
 *
 *  Created on: Jan 13, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "core/k_worker_thread.h"
#include "lib/vector.h"
#include "lib/refcount.h"

uint64_t current_work_id = 0;
vector<k_work::work*> work_queue;
vector<k_work::work*> finished_list;
process *worker_thread;

void k_worker_thread() {
	while(true) {
		while( work_queue.count() > 0 ) {
			k_work::work *current = work_queue.remove_end();
			current->return_code = current->func();
			current->finished = true;
			kprintf("Finished kernel deferred work.\n");

			if( !current->auto_remove ) {
				finished_list.add_end(current);
			}

			kprintf("Sending wakeup call.\n");
			message wakeup_call( "k_worker_thread_finished", (void*)current, 0 );
			send_message(wakeup_call);
		}
		k_work::reap_orphans();

	    set_message_listen_status( "k_worker_thread_ready", true );
	    unique_ptr<message> msg = wait_for_message( "k_worker_thread_ready" );
	    set_message_listen_status( "k_worker_thread_ready", false );
	}
}

k_work::work::work( unsigned int(*func)(void), bool auto_remove ) {
	this->spawning_pid = process_current->id;
	this->work_id = current_work_id++;
	this->func = func;
	this->return_code = 0;
	this->finished = false;
	this->auto_remove = auto_remove;
}

unsigned int k_work::work::wait() {
	set_message_listen_status( "k_worker_thread_finished", true );
	while(!this->finished) {
		kprintf("Waiting on kernel deferred work...\n");
		unique_ptr<message> msg = wait_for_message( "k_worker_thread_finished" );
		if(msg->data == (void*)this) {
			break;
		}
	}
	set_message_listen_status( "k_worker_thread_finished", false );

	if( !this->auto_remove ) {
		// remove the work object if we haven't already
		for(unsigned int i=0;i<finished_list.count();i++) {
			if( finished_list[i]->work_id == this->work_id ) {
				finished_list.remove(i);
				break;
			}
		}
	}

	unsigned int ret = this->return_code;

	delete this;

	return ret;
}

k_work::work* k_work::schedule( unsigned int(*func)(void), bool auto_remove ) {
	k_work::work *work = new k_work::work(func, auto_remove);
	work_queue.add_end(work);

	message wakeup_call( "k_worker_thread_ready", NULL, 0 );
	send_message( wakeup_call );

	return work;
}

void k_work::reap_orphans() {
	for(unsigned int i=0;i<finished_list.count();i++) {
		process* proc = get_process_by_pid(finished_list[i]->spawning_pid);
		if( (proc == NULL) || (proc->state == process_state::dead) ) {
			k_work::work *wk = finished_list.remove(i);
			delete wk;
		}
	}
}

void k_work::start() {
	worker_thread = new process( (uint32_t)&k_worker_thread, false, 0, "k_worker_thread", NULL, 0 );
	register_channel( "k_worker_thread_ready", CHANNEL_MODE_INV_MCAST, worker_thread );
	register_channel( "k_worker_thread_finished", CHANNEL_MODE_MULTICAST, worker_thread );
	spawn_process(worker_thread);
}
