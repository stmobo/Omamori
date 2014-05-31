// refcount.cpp

#include "includes.h"
#include "lib/vector.h"
#include "lib/refcount.h"

vector<bool> shared_ptr_is_alive(1);
mutex        shared_ptr_list_lock;

void refcount::increment() {
    this->lock.lock();
    this->count++;
    this->lock.unlock();
}

uint64_t refcount::decrement() {
    this->lock.lock();
    uint64_t ret = 0;
    if(this->count > 0) {
        this->count--;
        ret = this->count;
    }
    this->lock.unlock();
    return ret;
}

uint32_t get_next_shared_ptr_id() {
    shared_ptr_list_lock.lock();
    shared_ptr_is_alive.add(true);
    uint32_t ret = shared_ptr_is_alive.length() - 1;
    shared_ptr_list_lock.unlock();
    return ret;
}