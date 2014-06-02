// refcount.h -- reference-counting classes / pointers
#pragma once
#include "includes.h"
#include "lib/vector.h"

template<class T> class shared_ptr;
template<class T> class weak_ptr;
template<class T> class unique_ptr;

extern vector<bool> shared_ptr_is_alive;
extern mutex        shared_ptr_list_lock;

extern uint32_t get_next_shared_ptr_id();

typedef class refcount {
    mutex lock;
    uint64_t count = 0;
    
    public:
    void increment();
    uint64_t decrement();
} refcount;

//
//
//
//
//

template<class T>
class shared_ptr {
    T* object;
    refcount* rc; // if (this->rc != NULL) then (this->object != NULL)
    uint32_t  rc_id;
    
    public:
    const weak_ptr<T>&    operator=(const weak_ptr<T>&);
    const shared_ptr<T>&  operator=(const shared_ptr<T>&);
    shared_ptr<T>&        operator=(const T*);
    T&                    operator*()  { return *this->object; };
    T*                    operator->() { return this->object; };
    
    bool                  operator==(const weak_ptr<T>& rhs) { return (this->object == rhs.object); }
    bool                  operator==(const shared_ptr<T>& rhs) { return (this->object == rhs.object); }
    bool                  operator!=(const weak_ptr<T>& rhs) { return (this->object != rhs.object); }
    bool                  operator!=(const shared_ptr<T>& rhs) { return (this->object != rhs.object); }
    
    T*                    get() { return this->object; };
    void                  invalidate();
    
    shared_ptr() : object(NULL), rc(NULL), rc_id(0) {};
    shared_ptr(T*);
    shared_ptr(weak_ptr<T>&);
    shared_ptr(shared_ptr<T>&);
    ~shared_ptr();
};

template<class T>
shared_ptr<T>::shared_ptr(weak_ptr<T>& org) {
    if( !org.expired() ) {
        if( org.rc != NULL ) {
            this->rc = org.rc;
            this->rc->increment();
            this->rc_id = org.rc_id;
        }
        this->object = org.object;
    } else {
        this->object = NULL;
    }
}

template<class T>
shared_ptr<T>::shared_ptr(shared_ptr<T>& org) {
    if( org.rc != NULL ) {
        this->rc = org.rc;
        this->rc->increment();
        this->rc_id = org.rc_id;
    }
    this->object = org.object;
}

template<class T>
shared_ptr<T>::shared_ptr(T* obj) {
    this->rc = new refcount;
    this->rc->increment();
    this->rc_id = get_next_shared_ptr_id();
    this->object = obj;
}

template<class T>
shared_ptr<T>::~shared_ptr() {
    this->invalidate();
}

template<class T>
void shared_ptr<T>::invalidate() {
    if( this->rc != NULL ) {
        if( this->rc->decrement() == 0 ) {
            delete this->object;
            delete this->rc;
            shared_ptr_is_alive.set( this->rc_id, false );
        }
    }
    this->object = NULL;
    this->rc     = NULL;
    this->rc_id  = 0;
}

template<class T>
const weak_ptr<T>& shared_ptr<T>::operator=(const weak_ptr<T>& org) {
    this->invalidate();
    if( !org.expired() ) {
        if( org.rc != NULL ) {
            this->rc = org.rc;
            this->object = org.object;
            this->rc->increment();
            this->rc_id = org.rc_id;
        }
    }
    return org;
}

template<class T>
const shared_ptr<T>& shared_ptr<T>::operator=(const shared_ptr<T>& org) {
    if( &org != this ) {
        this->invalidate();
        if( org.rc != NULL ) {
            this->rc = org.rc;
            this->object = org.object;
            this->rc->increment();
            this->rc_id = org.rc_id;
        }
    }
    return const_cast<shared_ptr<T>&>(org);
}

template<class T>
shared_ptr<T>& shared_ptr<T>::operator=(const T* obj) {
    this->invalidate();
    if( obj != NULL ) {
        this->rc = new refcount;
        this->rc->increment();
        this->rc_id = get_next_shared_ptr_id();
    }
    this->object = const_cast<T*>(obj);
    return *this;
}

//
//
//
//
//

template<class T>
class weak_ptr {
    T* object;
    refcount* rc; // if (this->rc != NULL) then (this->object != NULL)
    uint32_t  rc_id;
    
    public:
    const shared_ptr<T>   operator=(const shared_ptr<T>);
    const weak_ptr<T>     operator=(const weak_ptr<T>);
    
    bool                  operator==(const weak_ptr<T>& rhs) { return (this->object == rhs.object); }
    bool                  operator==(const shared_ptr<T>& rhs) { return (this->object == rhs.object); }
    bool                  operator!=(const weak_ptr<T>& rhs) { return (this->object != rhs.object); }
    bool                  operator!=(const shared_ptr<T>& rhs) { return (this->object != rhs.object); }
    
    shared_ptr<T> get_shared();
    T*            get() { return this->object; };
    bool          expired();
    
    weak_ptr() : object(NULL), rc(NULL) {};
    weak_ptr(shared_ptr<T>&);
    weak_ptr(weak_ptr<T>&);
};

template<class T>
weak_ptr<T>::weak_ptr( shared_ptr<T>& ptr ) {
    this->object = ptr.object;
    this->rc     = ptr.rc;
    this->rc_id  = ptr.rc_id;
}

template<class T>
weak_ptr<T>::weak_ptr( weak_ptr<T>& ptr ) {
    this->object = ptr.object;
    this->rc     = ptr.rc;
    this->rc_id  = ptr.rc_id;
}

template<class T>
const weak_ptr<T> weak_ptr<T>::operator=( const weak_ptr<T> ptr ) {
    this->object = ptr.object;
    this->rc     = ptr.rc;
    this->rc_id  = ptr.rc_id;
    return ptr;
}

template<class T>
const shared_ptr<T> weak_ptr<T>::operator=( const shared_ptr<T> ptr ) {
    this->object = ptr.object;
    this->rc     = ptr.rc;
    this->rc_id  = ptr.rc_id;
    return ptr;
}

template<class T>
bool weak_ptr<T>::expired() {
    if( this->rc_id == 0 ) {
        return true;
    }
    return !shared_ptr_is_alive[ this->rc_id ];
}

template<class T>
shared_ptr<T> weak_ptr<T>::get_shared() {
    return shared_ptr<T>(this); // the shared_ptr(weak_ptr) constructor checks this->expired() itself
}

//
//
//
//
//

template<class T>
class unique_ptr {
    T* object;
    
    public:
    void        operator=(const unique_ptr<T>);
    void        operator=(const T*);
    T&          operator*()  { return *this->object; };
    T           operator->() { return this->object; };
    T*          get() { return this->object; };
    
    unique_ptr() : object(NULL) {};
    unique_ptr(T* obj) : object(obj) {};
    unique_ptr(unique_ptr<T>&);
    ~unique_ptr() { if(this->object != NULL) { delete this->object; } };
};

template<class T>
unique_ptr<T>::unique_ptr(unique_ptr<T>& ptr) {
    this->object = ptr.object;
    ptr.object = NULL;
}

template<class T>
void unique_ptr<T>::operator=(const unique_ptr<T> ptr) {
    if(this->object != NULL)
        delete this->object;
    this->object = ptr.object;
    ptr.object = NULL;
}

template<class T>
void unique_ptr<T>::operator=(const T* ptr) {
    if(this->object != NULL)
        delete this->object;
    this->object = ptr;
}