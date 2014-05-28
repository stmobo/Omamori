// sync.h
#pragma once
#include "includes.h"

#define SPINLOCK_LOCKED_VALUE               0x0010CCED
#define SPINLOCK_UNLOCKED_VALUE             0

typedef class spinlock {
    uint32_t lock_value;
    uint32_t locker;
    bool int_status;
    
    public:
    spinlock();
    bool get_lock_status();
    uint32_t get_lock_owner();
    void lock();
    void unlock();
    void lock_cli();
    void unlock_cli();
} spinlock;

typedef class reentrant_mutex {
    spinlock *control_lock;
    uint32_t lock_count;
    int uid;
    
    public:
    int get_owner_uid();
    uint32_t get_lock_count();
    
    bool trylock( int uid );
    bool trylock( );
    
    void lock( int uid );
    void unlock( int uid );
    
    void lock();
    void unlock();
    
    ~reentrant_mutex();
    reentrant_mutex();
} mutex;

typedef class semaphore {
    spinlock *control_lock;
    uint32_t count;
    uint32_t max_count;
    
    public:
    uint32_t get_count();
    uint32_t get_max_count();
    bool try_acquire( uint32_t count );
    void acquire( uint32_t count );
    bool release( uint32_t count );
    ~semaphore();
    semaphore();
    semaphore(uint32_t);
    semaphore(uint32_t,uint32_t);
} semaphore;
