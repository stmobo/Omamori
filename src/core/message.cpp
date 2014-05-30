// message.cpp - message passing system

#include "includes.h"
#include "lib/hash_table.h"
#include "lib/vector.h"
#include "core/scheduler.h"

// maybe a hash table mapping input strings to vector<process*>'s instead?
hash_table< vector<process*>* >* message_queues;

void wake_all_in_queue( char *queue_name ) {
    vector<process*>* queue = (*message_queues)[queue_name];
    if(queue) {
        for(int i=0;i<queue->length();i++) {
            (*queue)[i]->state = process_state::runnable;
            process_add_to_runqueue( (*queue)[i] );
        }
    }
}

void send_all_in_queue( char *queue_name, message msg ) {
    vector<process*>* queue = (*message_queues)[queue_name];
    if(queue) {
        for(int i=0;i<queue->length();i++) {
            (*queue)[i]->send_message( msg );
        }
    }
}

void send_message( message msg ) {
    vector<process*> *queue = (*message_queues)[const_cast<char*>(msg.type)];
    if(queue) {
        int processes_recv = 0;
        //kprintf("send_message: queue->length() = %u.\n", (unsigned long long int)queue->length());
        for(int i=0;i<queue->length();i++) {
            process *current = queue->get(i);
            if( current != NULL ) {
                current->send_message( msg );
                current->state = process_state::runnable;
                process_add_to_runqueue( current );
                processes_recv++;
            }
        }
    }
}

// yes, you have to delete the returned pointer.
message *get_latest_message() {
    process_current->message_queue_lock.lock();
    message *ret = process_current->message_queue.remove();
    process_current->message_queue_lock.unlock();
    return ret;
}

bool process::send_message( message msg ) {
    if( (this->state == process_state::waiting) || (this->state == process_state::runnable) ) {
        message *copy = new message;
        copy->type = msg.type;
        copy->data = msg.data;
        this->message_queue.add(copy);
        return true;
    }
    return false;
}

message *wait_for_message() {
    //kprintf("process %u (%s): Waiting for message!\n", (unsigned long long int)process_current->id, process_current->name);
    process_current->state = process_state::waiting;
    while( true ) {
        if( process_current->message_queue.length() > 0 ) {
            //kprintf("wait_for_message: length is now %u.\n", (unsigned long long int)process_current->message_queue.length());
            break;
        }
        process_switch_immediate();
    }
    process_current->state = process_state::runnable;
    message *ret = NULL;
    int n_times_cycled = 0;
    process_current->message_queue_lock.lock();
    while( true ) {
        ret = process_current->message_queue.remove();
        if( ret != NULL )
            break;
        n_times_cycled++;
    }
    process_current->message_queue_lock.unlock();
    //kprintf("process %u (%s): Got a message of type %s! (cycled %u times)\n", (unsigned long long int)process_current->id, process_current->name, ret->type, (unsigned long long int)n_times_cycled);
    return ret;
}

bool set_event_listen_status( char* event_name, bool status ) {
    vector<process*>* queue = (*message_queues)[event_name];
    if(queue) {
        if(status) {
            for(int i=0;i<queue->length();i++) {
                process *current = queue->get(i);
                if( current != NULL ) {
                    if( current->id == process_current->id ) {
                        return true;
                    }
                }
            }
            //kprintf("message: adding process %u to queue.\n", (unsigned long long int)process_current->id);
            queue->add( process_current );
        } else {
            for(int i=0;i<queue->length();i++) {
                process *current = queue->get(i);
                if( current != NULL ) {
                    if( current->id == process_current->id ) {
                        queue->remove(i);
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void register_event_type( char* event_name ) {
    vector<process*>* new_queue = new vector<process*>;
    if( !new_queue )
        panic("messaging: could not allocate space for new message recipient list!");
    message_queues->set( event_name, new_queue );
}

message::message() {
    this->timestamp = get_sys_time_counter();
}

void initialize_ipc() {
    message_queues = new hash_table< vector<process*>* >(0x1000);
}