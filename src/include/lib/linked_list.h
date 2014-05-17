#pragma once
#include "includes.h"

template <class list_obj_type>
class linked_list {
    private:
        linked_list* next;
        linked_list* prev;
    public:
        list_obj_type* elem;
        linked_list* append();
        int n_elements();
        list_obj_type* operator[](int);
        linked_list(linked_list*,linked_list*);
        ~linked_list();
};

//
//
//
//
//

template <class list_obj_type>
linked_list<list_obj_type>::~linked_list() {
    this->prev->next = this->next;
    this->next->prev = this->prev;
    kfree((char*)this->elem);
}

template <class list_obj_type>
linked_list<list_obj_type>::linked_list(linked_list* next,linked_list* prev) {
    this->next = next;
    this->prev = prev;
    this->next->prev = this;
    this->prev->next = this;
    this->elem = (list_obj_type*) new list_obj_type;
}

template <class list_obj_type>
int linked_list<list_obj_type>::n_elements() {
    int i=1;
    linked_list *obj = this;
    while(obj->next != NULL) {
        obj = obj->next;
        i++;
    }
    return i;
}

template <class list_obj_type>
linked_list<list_obj_type>* linked_list<list_obj_type>::append() {
    linked_list *obj = this;
    while(obj->next != NULL) {
        obj = obj->next;
    }
    linked_list *newobj = new linked_list<list_obj_type>(NULL, this);
    return newobj;
}

template <class list_obj_type>
list_obj_type* linked_list<list_obj_type>::operator[](int n) {
    linked_list *obj = this;
    if(n > 0) {
        for(int i=0;i<n;i++) {
            obj = obj->next;
            if(obj == NULL) {
                return NULL;
            }
        }
    } else if(n < 0) {
        for(int i=0;i>n;i--) {
            obj = obj->prev;
            if(obj == NULL) {
                return NULL;
            }
        }
    }
    return obj->elem;
}