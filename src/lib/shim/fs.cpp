/*
 * fs.cpp
 *
 *  Created on: Jan 26, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "lib/shim/computercraft.h"

namespace computercraft {
	static int fs_list( lua_State *L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vector<vfs_node*> listing;

		vfs::vfs_status fop_stat = vfs::list_directory(path, &listing);
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		}

		lua_createtable( L, listing.count(), 0 );
		for(unsigned int i=0;i<listing.count();i++) {
			lua_pushstring( L, const_cast<const char*>((char*)listing[i]->name) );
			lua_rawseti( L, -2, i+1 );
		}
		return 1;
	}

	static int fs_exists( lua_State *L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		if( vfs::file_exists(path) ) {
			lua_pushboolean( L, 1 );
		} else {
			lua_pushboolean( L, 0 );
		}
		return 1;
	}

	static int fs_isDir( lua_State *L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vfs_node* node = NULL;

		vfs::vfs_status fop_stat = vfs::get_file_info( path, &node );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			lua_pushboolean( L, (node->type == vfs_node_types::directory) );
			return 1;
		}
	}

	static int fs_isReadOnly( lua_State* L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vfs_node* node = NULL;

		vfs::vfs_status fop_stat = vfs::get_file_info( path, &node );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			lua_pushboolean( L, node->attr.read_only );
			return 1;
		}
	}

	static int fs_getSize( lua_State* L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vfs_node* node = NULL;

		vfs::vfs_status fop_stat = vfs::get_file_info( path, &node );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			if( node->type == vfs_node_types::file ) {
				vfs_file* f = (vfs_file*)node;
				lua_pushnumber( L, (double)f->size );
				return 1;
			} else {
				lua_pushnil( L );
				lua_pushstring( L, "Incorrect type" );
				return 2;
			}
		}
	}

	static int fs_makeDir( lua_State *L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vfs::vfs_status fop_stat = vfs::create_directory( path );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			return 0;
		}
	}

	static int fs_move( lua_State *L ) {
		const char* p1 = luaL_checkstring(L, 1);
		const char* p2 = luaL_checkstring(L, 2);
		unsigned char* to = (unsigned char*)const_cast<char*>(p2);
		unsigned char* from = (unsigned char*)const_cast<char*>(p1);

		vfs::vfs_status fop_stat = vfs::move_file( to, from );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			return 0;
		}
	}

	static int fs_copy( lua_State *L ) {
		const char* p1 = luaL_checkstring(L, 1);
		const char* p2 = luaL_checkstring(L, 2);
		unsigned char* to = (unsigned char*)const_cast<char*>(p2);
		unsigned char* from = (unsigned char*)const_cast<char*>(p1);

		vfs::vfs_status fop_stat = vfs::copy_file( to, from );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			return 0;
		}
	}

	static int fs_delete( lua_State *L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vfs::vfs_status fop_stat = vfs::delete_file( path );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			return 0;
		}
	}

	static int fs_read( lua_State *L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vfs_node *node = NULL;

		vfs::vfs_status fop_stat = vfs::get_file_info( path, &node );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			if( node->type == vfs_node_types::file ) {
				vfs_file* f = (vfs_file*)node;
				void* d = kmalloc( f->size );
				vfs::read_file( path, d );

				lua_pushlstring( L, const_cast<const char*>((char*)d), f->size );
				return 1;
			} else {
				lua_pushnil( L );
				lua_pushstring( L, "Incorrect type" );
				return 2;
			}
		}
	}

	static int fs_write( lua_State *L ) {
		const char* p = luaL_checkstring(L, 1);
		unsigned char* path = (unsigned char*)const_cast<char*>(p);

		vfs_node *node = NULL;

		vfs::vfs_status fop_stat = vfs::get_file_info( path, &node );
		if( fop_stat != vfs::vfs_status::ok ) {
			lua_pushnil( L );
			lua_pushstring( L, vfs::status_description(fop_stat) );
			return 2;
		} else {
			if( node->type == vfs_node_types::file ) {
				size_t len = 0;
				const char* d = luaL_checklstring( L, 2, &len );
				void *d2 = (void*)(const_cast<char*>(d));

				fop_stat = vfs::write_file( path, d2, len );
				if( fop_stat != vfs::vfs_status::ok ) {
					lua_pushnil( L );
					lua_pushstring( L, vfs::status_description(fop_stat) );
					return 2;
				}

				return 0;
			} else {
				lua_pushnil( L );
				lua_pushstring( L, "Incorrect type" );
				return 2;
			}
		}
	}

	void luaopen_cc_fs( lua_State* L ) {
		lua_createtable( L, 0, 11 );

		lua_pushstring(L, "list");
		lua_pushcfunction(L, fs_list);
		lua_rawset( L, -2 );

		lua_pushstring(L, "exists");
		lua_pushcfunction(L, fs_exists);
		lua_rawset( L, -2 );

		lua_pushstring(L, "isDir");
		lua_pushcfunction(L, fs_isDir);
		lua_rawset( L, -2 );

		lua_pushstring(L, "isReadOnly");
		lua_pushcfunction(L, fs_isReadOnly);
		lua_rawset( L, -2 );

		lua_pushstring(L, "getSize");
		lua_pushcfunction(L, fs_getSize);
		lua_rawset( L, -2 );

		lua_pushstring(L, "makeDir");
		lua_pushcfunction(L, fs_makeDir);
		lua_rawset( L, -2 );

		lua_pushstring(L, "move");
		lua_pushcfunction(L, fs_move);
		lua_rawset( L, -2 );

		lua_pushstring(L, "copy");
		lua_pushcfunction(L, fs_copy);
		lua_rawset( L, -2 );

		lua_pushstring(L, "delete");
		lua_pushcfunction(L, fs_delete);
		lua_rawset( L, -2 );

		lua_pushstring(L, "read");
		lua_pushcfunction(L, fs_read);
		lua_rawset( L, -2 );

		lua_pushstring(L, "write");
		lua_pushcfunction(L, fs_write);
		lua_rawset( L, -2 );

		lua_setglobal( L, "fs" );
	}
}
