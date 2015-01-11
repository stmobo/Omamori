// vector.h -- vector templated class
#pragma once
#include "core/sys.h"
#include "core/dynmem.h"
#include "lib/sync.h"

template <class T>
class vector {
    T* elements;
    unsigned int n_allocated_for = 0;
    unsigned int n_elements = 0;
    
    public:
    void reallocate( unsigned int );
    unsigned int count() { return this->n_elements; };
    unsigned int length() { return this->n_allocated_for; };
    void clear();
    
    // array access-style functions
    void set( unsigned int, T );
    T get( unsigned int );
    T operator[](unsigned int);
    
    // FILO / FIFO -style functions
    void add( T );
    void add_end( T );
    T remove_end(); // removes from end   (FILO)
    T remove(unsigned int n=0);     // removes from start (FIFO)
    
    const vector<T>& operator=( const vector<T>& );
    vector( vector<T>& );
    vector();
    vector( unsigned int );
    ~vector();
};

template <class T>
vector<T>::vector() {
    this->elements = NULL;
    this->n_allocated_for = 0;
    this->n_elements = 0;
}

template <class T>
vector<T>::vector( vector<T>& original ) {
    this->elements = (T*)kmalloc(sizeof(T)*original.n_allocated_for);
    this->n_allocated_for = original.n_allocated_for;
    this->n_elements = original.n_elements;
    for( unsigned int i=0;i<original.n_allocated_for;i++ ) {
        this->elements[i] = original.elements[i];
    }
}

template <class T>
vector<T>::vector( unsigned int n_elem ) {
    this->elements = (T*)kmalloc(sizeof(T)*n_elem);
    this->n_allocated_for = n_elem;
    this->n_elements = n_elem;
}

template <class T>
vector<T>::~vector() {
    if(this->elements != NULL)
        kfree(this->elements);
}

template <class T>
const vector<T>& vector<T>::operator=( const vector<T>& original ) {
    if( this->n_allocated_for > 0 )
        this->clear();
        
    this->elements = (T*)kmalloc(sizeof(T)*(original.n_allocated_for));
    this->n_allocated_for = original.n_allocated_for;
    this->n_elements = original.n_elements;
    for( unsigned int i=0;i<original.n_allocated_for;i++ ) {
        this->elements[i] = original.elements[i];
    }
    
    return original;
}

template <class T>
void vector<T>::reallocate( unsigned int n ) {
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
void vector<T>::set( unsigned int n, T obj ) {
    if( n >= this->n_allocated_for )
        this->reallocate( (n+1) - this->n_allocated_for ); // the internal array now has (n+1) elements.
    this->elements[n] = obj;
}

template <class T>
T vector<T>::get( unsigned int n ) {
    return this->elements[n];
}

template <class T>
T vector<T>::operator[](unsigned int n) {
    return (T)this->get(n);
}

template <class T>
void vector<T>::add( T obj ) {
    this->n_elements++;
    if( this->elements == NULL ) {
        this->reallocate( this->n_elements );
    } else if( this->n_elements > this->n_allocated_for ) {
        this->reallocate( this->n_elements - this->n_allocated_for );
    }
    for(unsigned int i=this->n_elements-1;i>=1;i--) {
        this->elements[i] = this->elements[i-1];
    }
    this->elements[0] = obj;
}

template <class T>
void vector<T>::add_end( T obj ) {
    this->n_elements++;
    if( this->elements == NULL ) {
        this->reallocate( this->n_elements );
    } else if( this->n_elements > this->n_allocated_for ) {
        this->reallocate( this->n_elements - this->n_allocated_for );
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
T vector<T>::remove( unsigned int n ) {
    if( this->n_elements > n ) {
        T elem = this->elements[n];
        for(unsigned int i=n;i<(this->n_elements-1);i++) {
            this->elements[i] = this->elements[i+1];
        }
        this->n_elements--;
        return elem;
    }
    return NULL;
}
