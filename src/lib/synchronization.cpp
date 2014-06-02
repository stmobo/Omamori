// synchronize.cpp -- multithreaded synchronization
// really, we don't need this quite yet -- we don't have SMP support
// but oh well.

#include "includes.h"
#include "lib/sync.h"
#include "arch/x86/multitask.h"
#include "core/scheduler.h"

#define cas __sync_bool_compare_and_swap

uint64_t last_mutex_id = 0;
uint64_t last_semaphore_id = 0;

spinlock::spinlock() {
    this->lock_value = SPINLOCK_UNLOCKED_VALUE;
}

// granted, this "spinlock" isn't really a spinlock at all, but this is a single-processor OS, so...

// Lock / Unlock, No interrupt disabling
void spinlock::lock() {
    if(multitasking_enabled) { // No point in locking if we're the only thing running THIS early on
        if( (process_current != NULL) && (this->locker != process_current->id) ) { // don't lock if we've already locked this lock, but be safe about it
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

// Lock / Unlock w/ interrupt disabling
uint32_t retaddr_panic;
extern spinlock __kprintf_lock;
void *errant_lock;
void spinlock::lock_cli() {
    if( this == NULL ) {
        panic("lock_cli: this==NULL!\n");
    }
    if (multitasking_enabled && (process_current != NULL) ) {
        if( this->locker != process_current->id ) {
            /*
            if( this != &__kprintf_lock ) {
                kprintf("%s (%u): locking spinlock.\n", process_current->name, (unsigned long long int)(process_current->id));
                errant_lock = this;
            }
            */
            while(!cas(&this->lock_value, SPINLOCK_UNLOCKED_VALUE, SPINLOCK_LOCKED_VALUE)) {
                /*
                if( this != &__kprintf_lock ) {
                    process *locker_process = get_process_by_pid( this->locker );
                    if(locker_process)
                        kprintf("%s (%u): waiting on spinlock held by %u (%s).\n", process_current->name, (unsigned long long int)(process_current->id), (unsigned long long int)locker_process->id, locker_process->name);
                }
                */
                process_switch_immediate();
                //asm volatile("pause" : : : "memory");
            }
        
            if( process_current != NULL ) {
                this->int_status = interrupts_enabled();
                asm volatile("cli" : : : "memory");
                this->locker = process_current->id;
            }
        }
    }
}

void spinlock::unlock_cli() {
    if(multitasking_enabled) {
        asm volatile("" : : : "memory");
        /*
        if( this != &__kprintf_lock ) {
            kprintf("%s (%u): unlocking spinlock.\n", process_current->name, (unsigned long long int)(process_current->id));
        }
        */
        this->lock_value = SPINLOCK_UNLOCKED_VALUE;
        this->locker = 0;
        if(this->int_status) {
            asm volatile("sti" : : : "memory");
        }
    }
}

bool spinlock::get_lock_status() {
    asm volatile("" : : : "memory");
    return (this->lock_value == SPINLOCK_LOCKED_VALUE);
}

uint32_t spinlock::get_lock_owner() {
    asm volatile("" : : : "memory");
    return this->locker;
}

reentrant_mutex::~reentrant_mutex() {
    //delete this->control_lock;
}

reentrant_mutex::reentrant_mutex() {
    /*
    this->control_lock = new spinlock;
    if( this->control_lock == NULL ) {
        panic("mutex: could not initialize control lock!");
    }
    */
    this->uid = -1;
    this->lock_count = 0;
    this->mutex_id = last_mutex_id++;
}

bool reentrant_mutex::trylock() {
    if( multitasking_enabled ) {
        this->control_lock.lock_cli();
        if( (this->uid == ~0) || (this->uid == process_current->id) ){
            this->uid = process_current->id;
            this->lock_count++;
            this->control_lock.unlock_cli();
            return true;
        }
        this->control_lock.unlock_cli();
        return false;
    }
    return true;
}

void reentrant_mutex::lock() {
    if( process_current != NULL) {
        if( multitasking_enabled ) { // don't screw up state if multitasking is not enabled
            if(this == NULL) {
                panic("mutex: attempted to dereference NULL mutex.\n");
            }
            /*
            if(this->control_lock == NULL) {
                //this->control_lock = new spinlock;
                panic_lock = (uint32_t)this;
                panic("mutex: lock not initialized?!\n");
            }
            */
            this->control_lock.lock_cli();
            if( (this->uid == ~0) || (this->uid == process_current->id) ){ // okay, it's not taken yet (or we've already taken it), acquire it
                this->uid = process_current->id;
            } else {
                this->control_lock.unlock_cli();
                while( true ) {
                    process_switch_immediate(); // go to sleep
                    this->control_lock.lock_cli(); // okay, checking again
                    if( (this->uid == ~0) || (this->uid == uid) ) { // is it still locked?
                        this->uid = process_current->id; // okay it is, acquire it
                        break;
                    } else {
                        process *locker_process = get_process_by_pid( this->uid );
                        if( locker_process ) {
                            if( locker_process->state == process_state::dead ) {
                                this->uid = process_current->id;
                                break;
                            }
                        }
                        this->control_lock.unlock_cli(); // no, it's not, release and go to sleep again
                    }
                }
                // control_lock is LOCKED.
            }
            // this->uid == uid
            this->lock_count++;
            this->control_lock.unlock_cli();
        }
    }
}

void reentrant_mutex::unlock() {
    if( process_current != NULL) {
        if( multitasking_enabled ) {
            this->control_lock.lock_cli();
            if(this->uid == process_current->id) {
                if( this->lock_count == 0 ) {
                    panic("sync: attempted to unlock mutex more times than it was acquired!");
                }
                this->lock_count--;
                if( this->lock_count == 0 ) {
                    this->uid = -1;
                }
            }
            this->control_lock.unlock_cli();
        }
    }
}

uint32_t reentrant_mutex::get_lock_count() {
    return this->lock_count;
}

int reentrant_mutex::get_owner_uid() {
    return this->uid;
}

semaphore::~semaphore() {
    //delete this->control_lock;
}

semaphore::semaphore() {
    this->count = 0;
    this->max_count = 0;
    this->semaphore_id = last_semaphore_id++;
    /*
    this->control_lock = new spinlock;
    if( this->control_lock == NULL ) {
        panic("semaphore: could not initialize control lock!");
    }
    */
}

semaphore::semaphore(uint32_t max) {
    this->count = max;
    this->max_count = max;
    this->semaphore_id = last_semaphore_id++;
    /*
    this->control_lock = new spinlock;
    if( this->control_lock == NULL ) {
        panic("semaphore: could not initialize control lock!");
    }
    */
}

semaphore::semaphore(uint32_t count, uint32_t max) {
    this->count = count;
    this->max_count = max;
    this->semaphore_id = last_semaphore_id++;
    /*
    this->control_lock = new spinlock;
    if( this->control_lock == NULL ) {
        panic("semaphore: could not initialize control lock!");
    }
    */
}


bool semaphore::release(uint32_t count) {
    this->control_lock.lock_cli();
    
    // make sure we don't attempt to increment the count past what it's supposed to be
    if( (this->max_count == 0) || ((this->count + count) <= this->max_count) ) {
        this->count += count;
        this->control_lock.unlock_cli();
        return true;
    }
    
    this->control_lock.unlock_cli();
    return false;
}

bool semaphore::try_acquire(uint32_t count) {
    if( !(this->max_count == 0) && (count > this->max_count) ) {
        return false;
    }
    if( multitasking_enabled ) {
        this->control_lock.lock_cli();
        if( this->count >= count ) {
            this->count -= count;
            this->control_lock.unlock_cli();
            return true;
        }
        this->control_lock.unlock_cli();
        return false;
    }
    return true;
}

void semaphore::acquire(uint32_t count) {
    // make sure we don't deadlock the process by attempting to fulfill an impossible request
    if( !(this->max_count == 0) && (count > this->max_count) ) {
        return;
    }
    if( multitasking_enabled ) {
        while(true) {
            if(this == NULL) {
                panic("semaphore: attempted to dereference NULL semaphore.\n");
            }
            /*
            if(this->control_lock == NULL) {
                panic_lock = (uint32_t)this;
                panic("semaphore: lock not initialized?!\n");
            }
            */
            this->control_lock.lock_cli();
            if( this->count >= count ) {
                this->count -= count;
                this->control_lock.unlock_cli();
                break;
            }
            this->control_lock.unlock_cli();
            process_switch_immediate();
        }
    }
}

uint32_t semaphore::get_count() {
    return this->count;
}

uint32_t semaphore::get_max_count() {
    return this->max_count;
}