// message.cpp - message passing system

#include "includes.h"
#include "lib/hash_table.h"
#include "lib/vector.h"
#include "core/scheduler.h"

uint64_t last_message_id = 0;

hash_table< vector<process*>* >* message_queues;

void wake_all_in_queue( char *queue_name ) {
    vector<process*>* queue = (*message_queues)[queue_name];
    if(queue) {
        for(unsigned int i=0;i<queue->length();i++) {
            (*queue)[i]->state = process_state::runnable;
            process_add_to_runqueue( (*queue)[i] );
        }
    }
}

void send_all_in_queue( char *queue_name, message msg ) {
    vector<process*>* queue = (*message_queues)[queue_name];
    if(queue) {
        for(unsigned int i=0;i<queue->length();i++) {
            (*queue)[i]->send_message( msg );
        }
    }
}

void send_message( message msg ) {
    vector<process*> *queue = (*message_queues)[const_cast<char*>(msg.type)];
    if(queue) {
        int processes_recv = 0;
        //kprintf("send_message: queue->length() = %u.\n", (unsigned long long int)queue->length());
        for(unsigned int i=0;i<queue->length();i++) {
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
message* get_latest_message() {
    process_current->message_queue_lock.lock();
    message* ret = process_current->message_queue.remove();
    process_current->message_queue_lock.unlock();
    return ret;
}

bool process::send_message( message msg ) {
    if( this->state != process_state::dead ) {
        message* copy = new message( msg );
        if(copy->data != NULL) {
            this->message_queue.add(copy);
            return true;
        } else {
            delete copy;
            return false;
        }
    }
    return false;
}

message* wait_for_message() {
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
    message* ret;
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
    vector<process*>* queue = message_queues->get(event_name);
    if(queue) {
        if(status) {
            for(unsigned int i=0;i<queue->length();i++) {
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
            for(unsigned int i=0;i<queue->length();i++) {
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

void register_message_type( char* message_type ) {
    vector<process*>* new_queue = new vector<process*>;
    if( !new_queue )
        panic("messaging: could not allocate space for new message recipient list!");
    message_queues->set( message_type, new_queue );
}

message::message( message& org ) {
    this->type    = org.type;
    this->data_sz = org.data_sz;
    this->uid     = last_message_id++;
    this->data    = kmalloc(org.data_sz);
    if( this->data != NULL )
        memcpy( this->data, org.data, org.data_sz );
}

message::message( const char* type, void* data, size_t data_sz ) {
    this->type    = type;
    this->data_sz = data_sz;
    this->uid     = last_message_id++;
    this->data    = kmalloc(data_sz);
    if( this->data != NULL )
        memcpy( this->data, data, data_sz );
}

message::~message() {
    if(this->data != NULL)
        kfree(this->data);
}

void initialize_ipc() {
    message_queues = new hash_table< vector<process*>* >(0x1000);
}