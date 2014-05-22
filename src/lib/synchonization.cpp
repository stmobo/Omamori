// synchronize.cpp -- multithreaded synchronization
// really, we don't need this quite yet -- we don't have SMP support
// but oh well.

#include "includes.h"
#include "lib/sync.h"
#include "arch/x86/multitask.h"
#include "core/scheduler.h"

#define cas __sync_bool_compare_and_swap


spinlock::spinlock() {
    this->lock_value = SPINLOCK_UNLOCKED_VALUE;
}

void spinlock::lock() {
    if(multitasking_enabled) { // No point in locking if we're the only thing running THIS early on
        if( (process_current == NULL) || (this->locker != process_current->id) ) { // don't lock if we've already locked this lock, but be safe about it
            while(!cas(&this->lock_value, SPINLOCK_UNLOCKED_VALUE, SPINLOCK_LOCKED_VALUE)) {
                process_switch_immediate();
                //asm volatile("pause" : : : "memory");
            }
        
            if( process_current != NULL ) {
                this->locker = process_current->id;
            }
        }
    }
}

void spinlock::unlock() {
    if(multitasking_enabled) {
        asm volatile("" : : : "memory");
        this->lock_value = SPINLOCK_UNLOCKED_VALUE;
        this->locker = 0;
    }
}

uint32_t spinlock::get_lock_value() {
    asm volatile("" : : : "memory");
    return this->lock_value;
}

reentrant_mutex::reentrant_mutex() {
    this->control_lock = new spinlock;
    this->uid = ~0;
    this->lock_count = 0;
}

void reentrant_mutex::lock( int uid ) {
    if( multitasking_enabled ) { // don't screw up state if multitasking is not enabled
        this->control_lock->lock();
        if( (this->uid == ~0) ){ // okay, it's not taken yet, acquire it
            this->uid = uid;
        } else {
            this->control_lock->unlock();
            while( true ) {
                process_switch_immediate(); // go to sleep
                this->control_lock->lock(); // okay, checking again
                if( this->uid == ~0 ) { // is it still locked?
                    this->uid = uid; // okay it is, acquire it
                    break;
                } else {
                    this->control_lock->unlock(); // no, it's not, release and go to sleep again
                }
            }
            // control_lock is LOCKED.
        }
        // this->uid == uid
        this->lock_count++;
        this->control_lock->unlock();
    }
}

void reentrant_mutex::unlock( int uid ) {
    if( multitasking_enabled ) {
        this->control_lock->lock();
        if(this->uid == uid) {
            if( this->lock_count == 0 ) {
                panic("sync: attempted to unlock mutex more times than it was acquired!");
            }
            this->lock_count--;
            if( this->lock_count == 0 ) {
                this->uid = ~0;
            }
        }
        this->control_lock->unlock();
    }
}

void reentrant_mutex::lock() {
    if( process_current != NULL) {
        return this->lock( process_current->id );
    }
}

void reentrant_mutex::unlock() {
    if( process_current != NULL) {
        return this->unlock( process_current->id );
    }
}

uint32_t reentrant_mutex::get_lock_count() {
    return this->lock_count;
}

int reentrant_mutex::get_owner_uid() {
    return this->uid;
}

semaphore::semaphore() {
    this->count = 0;
    this->max_count = 0;
    this->control_lock = new spinlock;
}

semaphore::semaphore(uint32_t max) {
    this->count = max;
    this->max_count = max;
    this->control_lock = new spinlock;
}

semaphore::semaphore(uint32_t count, uint32_t max) {
    this->count = count;
    this->max_count = max;
    this->control_lock = new spinlock;
}


void semaphore::release(uint32_t count) {
    this->control_lock->lock();
    
    // make sure we don't attempt to increment the count past what it's supposed to be
    if( (this->max_count == 0) || ((this->count + count) <= this->max_count) ) {
        this->count += count;
    }
    
    
    this->control_lock->unlock();
}

void semaphore::acquire(uint32_t count) {
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

uint32_t semaphore::get_count() {
    return this->count;
}

uint32_t semaphore::get_max_count() {
    return this->max_count;
}