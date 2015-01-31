/*
 * computercraft.h
 *
 *  Created on: Jan 26, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/vfs.h"
#include "core/message.h"
#include "device/vga.h"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

namespace computercraft {
	void luaopen_cc_term( lua_State* L );
	void luaopen_cc_fs( lua_State* L );

	struct cc_process {
		vector< channel_receiver* > events;
		lua_State* state;

		~cc_process() { for(unsigned int i=0;i<this->events.count();i++) { delete this->events[i]; } lua_close(this->state); };
	};

	cc_process* create_cc_process();
	void event_loop( cc_process* proc );
	void load_file( cc_process* proc, unsigned char* file );
}
