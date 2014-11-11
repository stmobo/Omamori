// message.h

#pragma once
#include "includes.h"
#include "device/pit.h"
#include "lib/vector.h"
#include "core/scheduler.h"

class process; // see scheduler.h
extern process* process_current;

struct channel {
    vector<process*>* listeners;
    unsigned int     mode;
    process*         owner;
    
    ~channel() { delete this->listeners; }
    channel( unsigned int mode ) { this->listeners = new vector<process*>; this->owner = process_current; }
    channel( unsigned int mode, process* owner ) { this->listeners = new vector<process*>; this->owner = owner;}
};

typedef struct message {
    const char*  type;
    void*        data;
    size_t       data_sz;
    uint64_t     uid;
    uint32_t     sender;
    
    bool operator==( const message& rhs ) { return (this->uid == rhs.uid); };
    bool operator!=( const message& rhs ) { return (this->uid != rhs.uid); };
    bool operator>( const message& rhs )  { return (this->uid > rhs.uid); };
    bool operator<( const message& rhs )  { return (this->uid < rhs.uid); };
    bool operator<=( const message& rhs )  { return (this->uid <= rhs.uid); };
    bool operator>=( const message& rhs )  { return (this->uid >= rhs.uid); };
    
    ~message();
    message( message& );
    message( const char*, void*, size_t );
} message;

// everyone transmits to everyone else
#define CHANNEL_MODE_BROADCAST      0
// the owner transmits to everyone else
#define CHANNEL_MODE_MULTICAST      1
// everyone else transmits to the owner
#define CHANNEL_MODE_INV_MCAST      2
// the owner transmits to one other person
#define CHANNEL_MODE_UNICAST        3

extern bool set_message_listen_status( char*, bool );
extern bool get_message_listen_status( char* );
extern message* wait_for_message( char* type=NULL ); 
extern message* get_latest_message(); 
extern bool send_message( message );
extern void wake_all_in_queue( char* ) ;
extern void send_all_in_queue( char*, message );
extern void initialize_ipc();
extern void register_channel( char*, unsigned int, process* );
extern void register_channel( char*, unsigned int );