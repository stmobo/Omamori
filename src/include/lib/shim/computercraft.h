/*
 * computercraft.h
 *
 *  Created on: Jan 26, 2015
 *      Author: Tatantyler
 */

#pragma once
#include "includes.h"
#include "core/vfs.h"
#include "device/vga.h"

extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

namespace computercraft {
	void luaopen_cc_term( lua_State* L );
	void luaopen_cc_fs( lua_State* L );
}
