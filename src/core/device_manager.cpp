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
	device_node root;

	device_tree_node::device_tree_node() {
		this->global_id = global_ids++;
	}

	void initialize() {
		root.device_data = NULL;
		root.global_id = 0;
		root.child_id = 0;
		root.enabled = true;
		root.type = dev_type::computer;

		// add initial "processor" node
		device_node* init_processor = new device_node;
		init_processor->device_data = NULL;
		init_processor->global_id = global_ids++;
		init_processor->child_id = 0;
		init_processor->enabled = true;
		init_processor->type = dev_type::processor;

		root.children.add_end(init_processor);
	}

}


