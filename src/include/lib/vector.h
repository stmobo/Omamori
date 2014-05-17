#pragma once
#include "dynmem.h"
#include "core/sys.h"

template <class list_obj_type>
class vector {
    private:
        list_obj_type* elements;
        int num_elements;
        int elements_allocated_for;
        void relocate(signed int);
    public:
        list_obj_type operator[] (int);
        void append(int);
        void remove_from_end();
        int n_elements();
        vector(int);
        ~vector();
};

//
//
//
//
//

template <class list_obj_type>
int vector<list_obj_type>::n_elements() {
    return this->num_elements;
}

template <class list_obj_type>
vector<list_obj_type>::vector(int n) {
    list_obj_type* memblock = (list_obj_type*)kmalloc(sizeof(list_obj_type)*n);
    this->num_elements = this->elements_allocated_for = n;
    this->elements = memblock;
}

template <class list_obj_type>
vector<list_obj_type>::~vector() {
    kfree((char*)this->elements);
}

template <class list_obj_type>
void vector<list_obj_type>::relocate(signed int added_elements) {
    int new_n = this->num_elements + added_elements; // added_elements can be negative, remember.
    if(new_n < 0)
        return;
    list_obj_type* memblock = (list_obj_type*)kmalloc(sizeof(list_obj_type)*new_n);
    for(int i=0;i<this->num_elements;i++) {
        memblock[i] = this->elements[i];
    }
    kfree((char*)this->elements);
    this->elements_allocated_for = this->num_elements = new_n;
    this->elements = memblock;
}

template <class list_obj_type>
void vector<list_obj_type>::remove_from_end() {
    this->num_elements--;
}

template <class list_obj_type>
void vector<list_obj_type>::append(int new_elements) {
    if(new_elements <= 0)
        return;
    if((this->elements_allocated_for - this->num_elements) >= new_elements) {
        this->num_elements += new_elements;
    } else {
        this->relocate(new_elements);
    }
}

template <class list_obj_type>
list_obj_type vector<list_obj_type>::operator[](int index) {
    return this->elements[index];
}