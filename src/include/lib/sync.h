// sync.h
#pragma once

#define SPINLOCK_LOCKED_VALUE               0x0010CCED
#define SPINLOCK_UNLOCKED_VALUE             0

typedef class spinlock {
    uint32_t lock_value;
    
    public:
    spinlock();
    uint32_t get_lock_value();
    void lock();
    void unlock();
} spinlock;

typedef class reentrant_mutex {
    spinlock *control_lock;
    uint32_t lock_count;
    int uid;
    
    public:
    int get_owner_uid();
    uint32_t get_lock_count();
    void lock( int uid );
    void unlock( int uid );
    reentrant_mutex();
} mutex;

typedef class semaphore {
    spinlock *control_lock;
    uint32_t count;
    uint32_t max_count;
    
    public:
    uint32_t get_count();
    uint32_t get_max_count();
    void acquire( uint32_t count );
    void release( uint32_t count );
    semaphore();
    semaphore(uint32_t);
    semaphore(uint32_t,uint32_t);
} semaphore;
