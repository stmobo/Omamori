/*
 * apic.h
 *
 *  Created on: Jan 28, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"

extern uint64_t lapic_base;
bool lapic_detect();
void initialize_apics();
