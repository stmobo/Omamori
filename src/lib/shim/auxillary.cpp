/*
 * auxillary.cpp
 *
 *  Created on: Jan 27, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "lib/shim/computercraft.h"

namespace computercraft {

	lua_State* create_exec_context() {
		lua_State *st = luaL_newstate();

		luaL_openlibs( st );
		luaopen_cc_fs( st );
		luaopen_cc_term( st );

		return st;
	}

}


