// hash_table.h

#include "includes.h"
// note to self: implement resizing at some point

template<class T>
struct ht_bucket_entry {
    const char* key = NULL;
    T data;
    ht_bucket_entry* next = NULL;
};

template<class T>
class hash_table {
    ht_bucket_entry<T> **internal_array = NULL;
    int internal_array_size = 0;
    int n_known_elements = 0;
    
    public:
    hash_table( int );
    ~hash_table( );
    void set( char*, T );
    void remove( char* );
    T get( char* );
    T operator[](char*);
};

inline unsigned long sdbm_hash(char* str) {
    unsigned long ret = 0;
    while( *str++ ) {
        ret = (*str)+(ret<<6)+(ret<<16) - ret;
    }
    return ret;
}

template<class T>
hash_table<T>::hash_table( int array_size ) {
    this->internal_array = new ht_bucket_entry<T>*[array_size];
    this->internal_array_size = array_size;
}

template<class T>
hash_table<T>::~hash_table() {
    if(this->internal_array) {
        for(int i=0;i<this->internal_array_size;i++) {
            delete this->internal_array[i];
        }
        delete this->internal_array;
    }
}

template<class T>
void hash_table<T>::set( char* key, T value ) {
    unsigned long hash = sdbm_hash(key) % this->internal_array_size;
    ht_bucket_entry<T> *bucket = this->internal_array[hash];
    if( bucket == NULL ) {
        bucket = this->internal_array[hash] = new ht_bucket_entry<T>;
        bucket->key = key;
        bucket->data = value;
        n_known_elements++;
    } else {
        while( bucket != NULL ) {
            if( strcmp( const_cast<char*>(bucket->key), key, 0 ) ) {
                bucket->data = value;
                break;
            }
            if( bucket->next == NULL ) {
                bucket->next = new ht_bucket_entry<T>;
                bucket->next->key = key;
                bucket->next->data = value;
                n_known_elements++;
                break;
            }
            bucket = bucket->next;
        }
    }
}

template<class T>
void hash_table<T>::remove( char* key ) {
    unsigned long hash = sdbm_hash(key) % (this->internal_array_size);
    ht_bucket_entry<T> *prev = NULL;
    ht_bucket_entry<T> *bucket = this->internal_array[hash];
    while( bucket != NULL ) {
        if( strcmp( const_cast<char*>(bucket->key), const_cast<char*>(key), 0 ) ) {
            if( prev )
                prev->next = bucket->next;
            n_known_elements--;
            delete bucket;
            break;
        }
        prev = bucket;
        bucket = bucket->next;
    }
}

template<class T>
T hash_table<T>::get( char* key ) {
    unsigned long hash = sdbm_hash(key) % (this->internal_array_size);
    ht_bucket_entry<T> *bucket = this->internal_array[hash];
    if( bucket == NULL ) {
        return NULL;
    } else {
        while( bucket != NULL ) {
            if( strcmp( const_cast<char*>(bucket->key), const_cast<char*>(key), 0 ) )
                return bucket->data;
            bucket = bucket->next;
        }
    }
    return NULL;
}

template<class T>
T hash_table<T>::operator[](char* key) {
    return this->get(key);
}