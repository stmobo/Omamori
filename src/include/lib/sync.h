// sync.h
#pragma once

typedef class reentrant_mutex {
    bool control_lock;
    int lock_count;
    int uid;
    
    void lock( int uid );
    void unlock( int uid );
    reentrant_mutex();
} mutex;

typedef class semaphore {
    bool control_lock;
    int count;
    int max_count;
    
    void acquire( int count );
    void release( int count );
    semaphore();
    semaphore(int);
    semaphore(int,int);
} semaphore;

typedef class spinlock {
    bool locked;
    
    spinlock();
    void lock();
    void unlock();
} spinlock;