// message.h

#pragma once
#include "includes.h"
#include "device/pit.h"
#include "lib/vector.h"

typedef struct message {
    const char*  type;
    void*        data;
    size_t       data_sz;
    uint64_t     uid;
    
    ~message();
    message( message& );
    message( const char*, void*, size_t );
} message;

extern bool set_event_listen_status( char*, bool );
extern message* wait_for_message(); 
extern message* get_latest_message(); 
extern void send_message( message );
extern void wake_all_in_queue( char* ) ;
extern void send_all_in_queue( char*, message );
extern void initialize_ipc();
extern void register_message_type( char* );