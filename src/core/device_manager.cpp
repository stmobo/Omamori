/*
 * device_manager.cpp
 *
 *  Created on: Jan 31, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "core/device_manager.h"

namespace device_manager {

	uint64_t global_ids = 1;

	void initialize() {
		root.device_data = NULL;
		root.global_id = 0;
		root.child_id = 0;
		root.enabled = true;
		root.type = dev_type::computer;
	}

}


