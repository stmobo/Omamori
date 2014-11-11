// message.cpp - message passing system

#include "includes.h"
#include "lib/hash_table.h"
#include "lib/vector.h"
#include "core/scheduler.h"

uint64_t last_message_id = 0;

hash_table< channel* >* message_queues;

void wake_all_in_queue( char *queue_name ) {
    channel* ch = (*message_queues)[queue_name];
    if(ch) {
        vector<process*>* queue = ch->listeners;
        for(unsigned int i=0;i<queue->length();i++) {
            (*queue)[i]->state = process_state::runnable;
            process_add_to_runqueue( (*queue)[i] );
        }
    }
}

void send_all_in_queue( char *queue_name, message msg ) {
    channel* ch = (*message_queues)[queue_name];
    if(ch) {
        vector<process*>* queue = ch->listeners;
        for(unsigned int i=0;i<queue->length();i++) {
            (*queue)[i]->send_message( msg );
        }
    }
}

void sort_message_queue_internal( vector<message*>& queue, unsigned int l_index, unsigned int r_index ) {
    if( l_index >= r_index )
        return;
    if( (r_index - l_index) > 1 ) {
        unsigned int middle = (l_index+r_index)/2;
        //kprintf("message_queue_sort: l_index = %u / ", l_index);
        //kprintf("middle = %u / ", middle);
        //kprintf("r_index = %u\n", r_index);
        uint64_t pivot = ((queue[l_index]->uid)+(queue[middle]->uid)+(queue[r_index]->uid))/3;
        //uint64_t pivot = ((queue[l_index]->uid)+(queue[r_index]->uid))/2;
        message *tmp = NULL;
        unsigned int store_index = l_index;
        for( unsigned int i=l_index;i<r_index;i++ ) {
            if( queue[i]->uid <= pivot ) {
                tmp = queue[i];
                queue.set(i, queue[store_index]);
                queue.set(store_index, tmp);
                store_index++;
            }
        }
        tmp = queue[store_index];
        queue.set(store_index, queue[r_index-1]);
        queue.set(r_index-1, tmp);
        if( store_index > 0 )
            sort_message_queue_internal( queue, l_index, store_index-1 );
        return sort_message_queue_internal( queue, store_index+1, r_index );
    }
}

void sort_message_queue( vector<message*>& queue ) {
    if( queue.count() > 0 )
        return sort_message_queue_internal( queue, 0, queue.count()-1 );
}

bool send_message( message msg ) {
    channel* ch = (*message_queues)[const_cast<char*>(msg.type)];
    if( ch == NULL ) {
        panic("Attempted to send message to invalid channel %s!\n", msg.type);
    }
    // check to see if the channel's in unicast mode
    // if so, then also check to see if we're either the listener or the owner
    if( (ch->mode == CHANNEL_MODE_UNICAST) && ((ch->owner == NULL) || ((*ch->listeners)[0] == NULL) || !(((*ch->listeners)[0]->id == process_current->id) || (ch->owner->id == process_current->id))) )
        return false;
    if( (ch->mode == CHANNEL_MODE_MULTICAST) && ((ch->owner == NULL) || (ch->owner->id != process_current->id)) )
        return false;
    if(ch) {
        vector<process*>* queue = ch->listeners;
        int processes_recv = 0;
        //kprintf("send_message: queue->length() = %u.\n", (unsigned long long int)queue->length());
        for(unsigned int i=0;i<queue->count();i++) {
            process *current = queue->get(i);
            if( (current != NULL) && (current->id != process_current->id) ) {
                if( current->send_message( msg ) ) {
                    current->state = process_state::runnable;
                    process_add_to_runqueue( current );
                    processes_recv++;
                }
            }
        }
        if( (ch->owner != NULL) && (ch->owner->id != process_current->id) ) {
            if( ch->owner->send_message( msg ) ) {
                ch->owner->state = process_state::runnable;
                process_add_to_runqueue( ch->owner );
                processes_recv++;
            }
        }
        if(processes_recv > 0)
            return true;
    }
    return false;
}

// yes, you have to delete the returned pointer.
message* get_latest_message() {
    process_current->message_queue_lock.lock();
    sort_message_queue( *process_current->message_queue );
    message* ret = process_current->message_queue->remove();
    process_current->message_queue_lock.unlock();
    return ret;
}

bool process::send_message( message msg ) {
    if( this->state != process_state::dead ) {
        message* copy = new message( msg );
        if(copy != NULL) {
            this->message_queue->add_end(copy);
            return true;
        } else {
            delete copy;
            return false;
        }
    }
    return false;
}

message* wait_for_message( char *type ) {
    //kprintf("process %u (%s): Waiting for message!\n", (unsigned long long int)process_current->id, process_current->name);
    if( type != NULL )
        process_current->message_waiting_on = type; // juuuust in case we get preempted inbetween these two steps
    if( type != NULL ) {
        process_current->message_queue_lock.lock();
            
        sort_message_queue( *process_current->message_queue );
        for(int i=0;i<process_current->message_queue->count();i++) {
            //kprintf("[precheck] process %u received message of type %s\n", process_current->id, process_current->message_queue->get(i)->type);
            if( strcmp( const_cast<char*>(process_current->message_queue->get(i)->type), type, 0 ) ) {
                message *ret = process_current->message_queue->remove(i);
                process_current->message_waiting_on = NULL;
                process_current->message_queue_lock.unlock();
                return ret;
            }
        }
        
        process_current->message_queue_lock.unlock();
        process_current->state = process_state::waiting;
        
        //kprintf("[wait] process %u waiting for message of type %s\n", process_current->id, type);
        while(true) {
            process_switch_immediate();
            process_current->state = process_state::runnable;
            
            process_current->message_queue_lock.lock();
            
            sort_message_queue( *process_current->message_queue );
            for(int i=0;i<process_current->message_queue->count();i++) {
                //kprintf("[wait] process %u received message of type %s\n", process_current->id, process_current->message_queue->get(i)->type);
                if( strcmp( const_cast<char*>(process_current->message_queue->get(i)->type), type, 0 ) ) {
                    message *ret = process_current->message_queue->remove(i);
                    process_current->message_waiting_on = NULL;
                    process_current->message_queue_lock.unlock();
                    return ret;
                }
            }
            
            process_current->message_queue_lock.unlock();
        }
    } else {
        process_current->state = process_state::waiting;
        while( true ) {
            if( process_current->message_queue->length() > 0 ) {
                //kprintf("wait_for_message: length is now %u.\n", (unsigned long long int)process_current->message_queue.length());
                break;
            }
            process_switch_immediate();
        }
        process_current->state = process_state::runnable;
        message* ret;
        process_current->message_queue_lock.lock();
        //kprintf("wait_for_message: count is now %u.\n", process_current->message_queue->count());
        sort_message_queue( *process_current->message_queue );
        for(int i=0;i<process_current->message_queue->count();i++) {
            //kprintf("[wait-any] process %u received message of type %s\n", process_current->id, process_current->message_queue->get(i)->type);
            ret = process_current->message_queue->get(i);
            if( ret != NULL ) {
                process_current->message_queue->remove(i);
                break;
            }
        }
        process_current->message_queue_lock.unlock();
        //kprintf("process %u (%s): Got a message of type %s! (cycled %u times)\n", (unsigned long long int)process_current->id, process_current->name, ret->type, (unsigned long long int)n_times_cycled);
        return ret;
    }
}

bool get_message_listen_status( char* event_name ) {
    channel* ch = (*message_queues)[event_name];
    vector<process*>* queue = ch->listeners;
    for(unsigned int i=0;i<queue->length();i++) {
        if( queue->get(i)->id == process_current->id ) {
            return true;
        }
    }
    return false;
}

bool set_message_listen_status( char* event_name, bool status ) {
    channel* ch = (*message_queues)[event_name];
    if( ch == NULL ) {
        panic("Attempted to listen to invalid channel %s!\n", event_name);
    }
    if( (ch->mode == CHANNEL_MODE_UNICAST) && (ch->listeners->length() > 0) )
        return false;
    if( ch->mode == CHANNEL_MODE_INV_MCAST ) // an inverted multicast channel has no listeners except for the owner
        return false;
    if(ch) {
        vector<process*>* queue = ch->listeners;
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
            queue->add_end( process_current );
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

void unregister_channel( char* message_type ) {
    if( message_queues->get(message_type) != NULL ) {
        channel *chan = message_queues->get(message_type);
        if( chan->owner == process_current ) {
            delete chan;
            message_queues->set( message_type, NULL );
        }
    }
}

void register_channel( char* message_type, unsigned int mode, process* owner ) {
    channel* new_queue = new channel( mode, owner );
    if( message_queues->get(message_type) != NULL )
        return;
    if( !new_queue->listeners )
        panic("messaging: could not allocate space for new message recipient list!");
    message_queues->set( message_type, new_queue );
}

void register_channel( char* message_type, unsigned int mode ) {
    channel* new_queue = new channel( mode );
    if( message_queues->get(message_type) != NULL )
        return;
    if( !new_queue->listeners )
        panic("messaging: could not allocate space for new message recipient list!");
    message_queues->set( message_type, new_queue );
}

message::message( message& org ) {
    this->type    = org.type;
    this->data_sz = org.data_sz;
    this->uid     = last_message_id++;
    this->data    = kmalloc(org.data_sz);
    if( process_current != NULL )
        this->sender = process_current->id;
    else
        this->sender = 0;
    if( (this->data != NULL) && (this->data_sz > 0) )
        memcpy( this->data, org.data, org.data_sz );
}

message::message( const char* type, void* data, size_t data_sz ) {
    this->type    = type;
    this->data_sz = data_sz;
    this->uid     = last_message_id++;
    this->data    = kmalloc(data_sz);
    if( process_current != NULL )
        this->sender = process_current->id;
    else
        this->sender = 0;
    if( (this->data != NULL) && (this->data_sz > 0) )
        memcpy( this->data, data, data_sz );
}

message::~message() {
    if(this->data != NULL)
        kfree(this->data);
}

void initialize_ipc() {
    message_queues = new hash_table< channel* >(0x1000);
}