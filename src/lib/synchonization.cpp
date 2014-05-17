// synchronize.cpp -- multithreaded synchronization
// really, we don't need this quite yet -- we don't have SMP support
// but oh well.

#include "includes.h"
#include "lib/sync.h"

#define cas __sync_bool_compare_and_swap


spinlock::spinlock() {
    this->lock_value = SPINLOCK_UNLOCKED_VALUE;
}

void spinlock::lock() {
    while(!cas(&this->lock_value, SPINLOCK_UNLOCKED_VALUE, SPINLOCK_LOCKED_VALUE)) {
        asm volatile("pause" : : : "memory");
    }
}

void spinlock::unlock() {
    asm volatile("" : : : "memory");
    this->lock_value = SPINLOCK_UNLOCKED_VALUE;
}

reentrant_mutex::reentrant_mutex() {
    this->control_lock = new spinlock;
    this->uid = ~0;
    this->lock_count = 0;
}

void reentrant_mutex::lock( int uid ) {
    this->control_lock->lock();
    if( (this->uid == ~0) ){
        this->uid = uid;
    }
    if(this->uid == uid) {
        this->lock_count++;
    }
    this->control_lock->unlock();
}

void reentrant_mutex::unlock( int uid ) {
    this->control_lock->lock();
    if(this->uid == uid) {
        this->lock_count--;
        if( this->lock_count == 0 ) {
            this->uid = ~0;
        } else if( this->lock_count < 0 ) {
            panic("sync: attempted to unlock mutex more times than it was acquired!");
        }
    }
    this->control_lock->unlock();
}

semaphore::semaphore() {
    this->count = 0;
    this->max_count = 0;
    this->control_lock = new spinlock;
}

semaphore::semaphore(int max) {
    this->count = max;
    this->max_count = max;
    this->control_lock = new spinlock;
}

semaphore::semaphore(int count, int max) {
    this->count = count;
    this->max_count = max;
    this->control_lock = new spinlock;
}


void semaphore::release(int count) {
    this->control_lock->lock();
    
    // make sure we don't attempt to increment the count past what it's supposed to be
    if( (this->max_count == 0) || ((this->count + count) <= this->max_count) ) {
        this->count += count;
    }
    
    
    this->control_lock->unlock();
}

void semaphore::acquire(int count) {
    // make sure we don't deadlock the process by attempting to fulfill an impossible request
    if( !(this->max_count == 0) && (count > this->max_count) ) {
        return;
    }
    while(true) {
        this->control_lock->lock();
        if( this->count >= count ) {
            this->count -= count;
            this->control_lock->unlock();
            break;
        }
        this->control_lock->unlock();
        asm volatile("pause" : : : "memory");
    }
}