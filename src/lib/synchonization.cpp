// synchronize.cpp -- multithreaded synchronization
// really, we don't need this quite yet -- we don't have SMP support
// but oh well.

#include "includes.h"
#include "lib/sync.h"

#define cas __sync_bool_compare_and_swap


spinlock::spinlock() {
    this->locked = false;
}

void spinlock::lock() {
    while(!cas(this->locked, false, true)) {
        asm volatile("pause" : : : "memory");
    }
}

void spinlock::unlock() {
    asm volatile("" : : : "memory");
    *(this->lock) = false;
}

reentrant_mutex::reentrant_mutex() {
    this->control_lock = false;
    this->uid = ~0;
    this->lock_count = 0;
}

void reentrant_mutex::lock( int uid ) {
    spinlock_lock( &this->control_lock );
    if( (this->uid == ~0) ){
        this->uid = uid;
    }
    if(this->uid == uid) {
        this->lock_count++;
    }
    spinlock_unlock( &this->control_lock );
}

void reentrant_mutex::unlock( int uid ) {
    spinlock_lock( &this->control_lock );
    if(this->uid == uid) {
        this->lock_count--;
        if( this->lock_count == 0 ) {
            this->uid = ~0;
        } else if( this->lock_count < 0 ) {
            panic("sync: attempted to unlock mutex more times than it was acquired!");
        }
    }
    spinlock_unlock( &this->control_lock );
}

semaphore::semaphore() {
    this->count = 0;
    this->max_count = 0;
    this->control_lock = false;
}

semaphore::semaphore(int max) {
    this->count = max;
    this->max_count = max;
    this->control_lock = false;
}

semaphore::semaphore(int count, int max) {
    this->count = count;
    this->max_count = max;
    this->control_lock = false;
}


void semaphore::release(int count) {
    spinlock_lock( &this->control_lock );
    
    // make sure we don't attempt to increment the count past what it's supposed to be
    if( (this->max_count == 0) || ((this->count + count) <= this->max_count) ) {
        this->count += count;
    }
    
    
    spinlock_unlock( &this->control_lock );
}

void semaphore::acquire(int count) {
    // make sure we don't deadlock the process by attempting to fulfill an impossible request
    if( !(this->max_count == 0) && (count > this->max_count) ) {
        return;
    }
    while(true) {
        spinlock_lock( &this->control_lock );
        if( this->count >= count ) {
            this->count -= count;
            spinlock_unlock( &this->control_lock );
            break;
        }
        spinlock_unlock( &this->control_lock );
        asm volatile("pause" : : : "memory");
    }
}