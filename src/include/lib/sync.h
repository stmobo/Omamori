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
    spinlock          control_lock;
    uint32_t          lock_count = 0;
    unsigned int      uid = ~0;
    uint64_t          mutex_id = ~0;
    
    public:
    int get_owner_uid();
    uint32_t get_lock_count();

    bool trylock( );
    void lock();
    void unlock();
    
    ~reentrant_mutex();
    reentrant_mutex();
} mutex;

typedef class semaphore {
    spinlock control_lock;
    uint32_t count;
    uint32_t max_count;
    uint64_t semaphore_id = ~0;
    
    public:
    uint32_t get_count();
    uint32_t get_max_count();
    bool try_acquire( uint32_t count );
    bool acquire( uint32_t count );
    bool release( uint32_t count );
    ~semaphore();
    semaphore();
    semaphore(uint32_t);
    semaphore(uint32_t,uint32_t);
} semaphore;

template <class T>
class lock_guard {
    T* lock;
    
    public:
    lock_guard( T& lock ) { this->lock = &lock; this->lock->lock(); }
    ~lock_guard( ) { this->lock->unlock(); this->lock = NULL; }
};

template <>
class lock_guard<semaphore> {
    semaphore *lock;
    int n_elements;
    bool status = false;
    
    public:
    lock_guard( semaphore& lock, int n_elements ) { this->n_elements = n_elements; this->lock = &lock; this->status = this->lock->acquire(n_elements); }
    ~lock_guard() { this->lock->release( this->n_elements ); }
};