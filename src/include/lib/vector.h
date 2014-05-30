// vector.h -- vector templated class
#pragma once
#include "core/sys.h"
#include "core/dynmem.h"
#include "lib/sync.h"

template <class T>
class vector {
    T* elements;
    int n_allocated_for;
    int n_elements;
    
    public:
    void reallocate( int );
    int length() { return this->n_allocated_for; };
    void clear();
    
    // array access-style functions
    void set( int, T );
    T get( int );
    T operator[](int);
    
    // FILO / FIFO -style functions
    void add( T );
    void add_end( T );
    T remove_end(); // removes from end   (FILO)
    T remove(int);     // removes from start (FIFO)
    
    vector();
    vector( int );
    ~vector();
};

template <class T>
vector<T>::vector() {
    this->elements = NULL;
    this->n_allocated_for = 0;
    this->n_elements = 0;
}

template <class T>
vector<T>::vector( int n_elem ) {
    this->elements = (T*)kmalloc(sizeof(T)*n_elem);
    this->n_allocated_for = n_elem;
    this->n_elements = n_elem;
}

template <class T>
vector<T>::~vector() {
    if(this->elements)
        kfree(this->elements);
}

template <class T>
void vector<T>::reallocate( int n ) {
    this->elements = resize<T>( this->elements, this->n_allocated_for, this->n_allocated_for+n );
    this->n_allocated_for += n;
}

template <class T>
void vector<T>::clear() {
    if( this->elements )
        kfree(this->elements);
    this->n_allocated_for = 0;
    this->n_elements = 0;
}

template <class T>
void vector<T>::set( int n, T obj ) {
    if( n >= this->n_allocated_for )
        this->reallocate( (n+1) - this->n_allocated_for ); // the internal array now has (n+1) elements.
    this->elements[n] = obj;
}

template <class T>
T vector<T>::get( int n ) {
    if( n >= this->n_allocated_for )
        return NULL;
    return this->elements[n];
}

template <class T>
T vector<T>::operator[](int n) {
    return (T)this->get(n);
}

template <class T>
void vector<T>::add( T obj ) {
    this->n_elements++;
    if( this->elements == NULL ) {
        this->reallocate( (this->n_elements+1) );
    } else if( this->n_elements >= this->n_allocated_for ) {
        this->reallocate( (this->n_elements - this->n_allocated_for)+1 );
    }
    for(int i=this->n_elements-1;i>=1;i--) {
        this->elements[i] = this->elements[i-1];
    }
    this->elements[0] = obj;
}

template <class T>
void vector<T>::add_end( T obj ) {
    this->n_elements++;
    if( this->elements == NULL ) {
        this->reallocate( (this->n_elements+1) );
    } else if( this->n_elements >= this->n_allocated_for ) {
        this->reallocate( (this->n_elements - this->n_allocated_for)+1 );
    }
    this->elements[this->n_elements-1] = obj;
}

template <class T>
T vector<T>::remove_end() {
    if( this->n_elements > 0 ) {
        T head = this->elements[this->n_elements-1];
        this->n_elements--;
        return head;
    }
    return NULL;
}

template <class T>
T vector<T>::remove( int n=0 ) {
    if( this->n_elements > n ) {
        T elem = this->elements[n];
        for(int i=n;i<(this->n_elements-1);i++) {
            this->elements[i] = this->elements[i+1];
        }
        this->n_elements--;
        return elem;
    }
    return NULL;
}