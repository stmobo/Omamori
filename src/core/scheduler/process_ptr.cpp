/*
 * process_ptr.cpp
 *
 *  Created on: Jan 23, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "lib/vector.h"
#include "core/scheduler.h"

void process::add_reference( process_ptr* ptr ) {
	this->process_reference_lock.lock();

	bool found = false;
	for(unsigned int i=0;i<this->process_reflist.count();i++) {
		if( this->process_reflist[i] == ptr ) {
			found = true;
			break;
		}
	}
	if( !found ) {
		this->process_reflist.add_end(ptr);
	}

	this->process_reference_lock.unlock();
}

void process::remove_reference( process_ptr* ptr ) {
	this->process_reference_lock.lock();

	for(unsigned int i=0;i<this->process_reflist.count();i++) {
		if( this->process_reflist[i] == ptr ) {
			this->process_reflist.remove(i);
			break;
		}
	}

	this->process_reference_lock.unlock();
}

process_ptr::~process_ptr() {
	this->invalidated = true;
	this->raw->remove_reference(this);
	this->raw = NULL;
}

process_ptr::process_ptr( process* rhs ) {
	this->raw = NULL;
	this->invalidated = true;
	if( rhs != NULL ) {
		rhs->add_reference( this );
		this->raw = rhs;
		this->invalidated = false;
	}
}

process_ptr::process_ptr( const process_ptr& rhs ) {
	this->raw = NULL;
	this->invalidated = true;
	if( rhs.raw != NULL ) {
		rhs.raw->add_reference( this );
		this->raw = rhs.raw;
		this->invalidated = false;
	}
}

process_ptr& process_ptr::operator=( process*& rhs ) {
	if( this->valid() ) {
		this->raw->remove_reference(this);
	}
	this->raw = NULL;
	this->invalidated = true;
	if( rhs != NULL ) {
		rhs->add_reference( this );
		this->raw = rhs;
		this->invalidated = false;
	}
	return *this;
}

process_ptr& process_ptr::operator=( process_ptr& rhs ) {
	this->raw = NULL;
	this->invalidated = true;
	if( rhs.raw != NULL ) {
		rhs.raw->add_reference( this );
		this->raw = rhs.raw;
		this->invalidated = false;
	}
	return *this;
}

