// sync.h
#pragma once

#define SPINLOCK_LOCKED_VALUE               0x0010CCED
#define SPINLOCK_UNLOCKED_VALUE             0

typedef class spinlock {
    uint32_t lock_value;
    
    public:
    spinlock();
    void lock();
    void unlock();
} spinlock;

typedef class reentrant_mutex {
    spinlock *control_lock;
    int lock_count;
    int uid;
    
    public:
    void lock( int uid );
    void unlock( int uid );
    reentrant_mutex();
} mutex;

typedef class semaphore {
    spinlock *control_lock;
    int count;
    int max_count;
    
    public:
    void acquire( int count );
    void release( int count );
    semaphore();
    semaphore(int);
    semaphore(int,int);
} semaphore;
