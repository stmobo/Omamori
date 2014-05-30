// message.h

#pragma once
#include "includes.h"
#include "device/pit.h"

typedef struct message {
    const char* type;
    void*       data;
    uint64_t    timestamp;
    
    message( const char* ev_name, void* ev_data ) { this->type = ev_name; this->data = ev_data; this->timestamp = get_sys_time_counter(); };
    message();
} message;

extern bool set_event_listen_status( char*, bool );
extern message *wait_for_message(); 
extern message *get_latest_message(); 
extern void send_message( message );
extern void wake_all_in_queue( char* ) ;
extern void send_all_in_queue( char*, message );
extern void initialize_ipc();
extern void register_event_type( char* );