// vector.h -- vector templated class
template <class T>
class vector {
    T* elements;
    int n_allocated_for;
    
    public:
    void reallocate( int );
    int length() { return this->n_allocated_for; };
    void add_next_empty( T );
    void clear();
    
    // array access-style functions
    void set( int, T );
    T get( int );
    T operator[](int);
    
    // FILO / FIFO -style functions
    void add( T );
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
}

template <class T>
vector<T>::vector( int n_elem ) {
    this->elements = (T*)kmalloc(sizeof(T)*n_elem);
    this->n_allocated_for = n_elem;
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
}

template <class T>
void vector<T>::set( int n, T obj ) {
    if( this->n_allocated_for < (n+1) )
        this->reallocate( (n+1) - this->n_allocated_for ); // the internal array now has (n+1) elements.
    this->elements[n] = obj;
}

template <class T>
T vector<T>::get( int n ) {
    if( this->n_allocated_for < (n+1) )
        return NULL;
    return this->elements[n];
}

template <class T>
T vector<T>::operator[](int n) {
    return (T)this->get(n);
}

template <class T>
void vector<T>::add( T obj ) {
    return this->set( this->n_allocated_for+1, obj );
}

template <class T>
void vector<T>::add_next_empty( T obj ) {
    for( int i=0;i<this->length();i++ ) {
        if( this[i] == NULL ) {
            this.set(i, obj);
        }
    }
    // couldn't find an empty spot to put it, so just make a new one
    this->add( obj );
}

template <class T>
T vector<T>::remove_end() {
    if( this->n_allocated_for > 0 ) {
        T head = this->elements[this->n_allocated_for-1];
        this->reallocate( -1 );
        return head;
    }
    return NULL;
}

template <class T>
T vector<T>::remove( int n=0 ) {
    if( this->n_allocated_for > n ) {
        T elem = this->elements[n];
        for(int i=n;i<(this->n_allocated_for-1);i++) {
            this->elements[i] = this->elements[i+1];
        }
        this->reallocate( -1 );
        return elem;
    }
    return NULL;
}