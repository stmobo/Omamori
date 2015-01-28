/*
 * term.cpp
 *
 *  Created on: Jan 27, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "lib/shim/computercraft.h"

static vga_color cc_num_to_color( unsigned int col ) {
	default:
	case 1:
		return vga_color::COLOR_WHITE;
	case 2: // orange
		return vga_color::COLOR_LIGHT_RED;
	case 4:
		return vga_color::COLOR_MAGENTA;
	case 8:
		return vga_color::COLOR_LIGHT_BLUE;
	case 16:
		return vga_color::COLOR_YELLOW;
	case 32:
		return vga_color::COLOR_LIGHT_GREEN;
	case 64:
		return vga_color::COLOR_LIGHT_MAGENTA;
	case 128:
		return vga_color::COLOR_DARK_GREY;
	case 256:
		return vga_color::COLOR_LIGHT_GREY;
	case 512:
		return vga_color::COLOR_CYAN;
	case 1024:
		return vga_color::COLOR_RED;
	case 2048:
		return vga_color::COLOR_BLUE;
	case 4096:
		return vga_color::COLOR_BROWN;
	case 8192:
		return vga_color::COLOR_GREEN;
	case 16384:
		return vga_color::COLOR_RED;
	case 32768:
		return vga_color::COLOR_BLACK;
}

namespace computercraft {
	static int term_write( lua_State* L ) {
		const char* t = luaL_checkstring( L, 1 );

		terminal_writestring( const_cast<char*>(t) );

		return 0;
	}

	static int term_clear( lua_State* L ) {
		terminal_clear();
		return 0;
	}

	static int term_clearline( lua_State* L ) {
		terminal_clearline();
		return 0;
	}

	static int term_getCursorPos( lua_State* L ) {
		lua_pushnumber( L, (double)terminal_column );
		lua_pushnumber( L, (double)terminal_row );
		return 2;
	}

	static int term_setCursorPos( lua_State* L ) {
		double x = luaL_checknumber( L, 1 );
		double y = luaL_checknumber( L, 2 );

		terminal_column = (size_t)x;
		terminal_row = (size_t)y;

		return 0;
	}

	static int term_getSize( lua_State* L ) {
		lua_pushnumber( L, (double)VGA_WIDTH );
		lua_pushnumber( L, (double)VGA_HEIGHT );
		return 2;
	}

	static int term_scroll( lua_State* L ) {
		double n = luaL_checknumber( L, 1 );

		terminal_scroll( (int)n );

		return 0;
	}

	static int term_setTextColor( lua_State* L ) {
		double c = luaL_checknumber( L, 1 );

		terminal_color = (terminal_color & 0xF0) | cc_num_to_color( c );

		return 0;
	}

	static int term_setBackgroundColor( lua_State* L ) {
		double c = luaL_checknumber( L, 1 );

		terminal_color = (terminal_color & 0x0F) | (cc_num_to_color( c ) << 4);

		return 0;
	}

	void luaopen_cc_term( lua_State* L ) {
		lua_createtable( L, 0, 9 );

		lua_pushstring(L, "write");
		lua_pushcfunction(L, term_write);
		lua_rawset( L, -2 );

		lua_pushstring(L, "clear");
		lua_pushcfunction(L, term_clear);
		lua_rawset( L, -2 );

		lua_pushstring(L, "clearLine");
		lua_pushcfunction(L, term_clearLine);
		lua_rawset( L, -2 );

		lua_pushstring(L, "getCursorPos");
		lua_pushcfunction(L, term_getCursorPos);
		lua_rawset( L, -2 );

		lua_pushstring(L, "setCursorPos");
		lua_pushcfunction(L, term_setCursorPos);
		lua_rawset( L, -2 );

		lua_pushstring(L, "getSize");
		lua_pushcfunction(L, term_getSize);
		lua_rawset( L, -2 );

		lua_pushstring(L, "scroll");
		lua_pushcfunction(L, term_scroll);
		lua_rawset( L, -2 );

		lua_pushstring(L, "setTextColor");
		lua_pushcfunction(L, term_setTextColor);
		lua_rawset( L, -2 );

		lua_pushstring(L, "setBackgroundColor");
		lua_pushcfunction(L, term_setBackgroundColor);
		lua_rawset( L, -2 );

		lua_setglobal( L, "term" );
	}
}
