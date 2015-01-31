/*
 * auxillary.cpp
 *
 *  Created on: Jan 27, 2015
 *      Author: Tatantyler
 */

#include "includes.h"
#include "lib/shim/computercraft.h"
#include "device/pit.h"
#include "device/ps2_keyboard.h"

namespace computercraft {

	lua_State* create_exec_context() {
		lua_State *st = luaL_newstate();

		luaL_openlibs( st );
		luaopen_cc_fs( st );
		luaopen_cc_term( st );

		return st;
	}

	static int startTimer( lua_State *L ) {
		lua_Number n = luaL_checknumber( L, 1 );

		unsigned int t = start_timer( (uint64_t)n );

		lua_pushnumber( L, t );

		return 1;
	}

	cc_process* create_cc_process() {
		lua_State *st = create_exec_context();

		lua_createtable( st, 0, 1 );

		lua_pushstring(st, "startTimer");
		lua_pushcfunction(st, startTimer);
		lua_rawset( st, -2 );

		lua_setglobal( st, "os" );

		vfs_node* node = NULL;
		vfs::vfs_status fop_stat = vfs::get_file_info( (unsigned char*)const_cast<char*>("/cd/INIT.LUA"), &node );
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("init.lua: FS get info failed with error: %s\n", vfs::status_description(fop_stat));
			return NULL;
		}

		if( node != NULL ) {
			if( node->type != vfs_node_types::file ) {
				kprintf("init.lua: FS read failed with error: incorrect type\n");
				return NULL;
			}
		}

		vfs_file* fn = (vfs_file*)node;
		void* buffer = kmalloc( fn->size );

		fop_stat = vfs::read_file( (unsigned char*)const_cast<char*>("/cd/INIT.LUA"), buffer );
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("init.lua: FS read failed with error: %s\n", vfs::status_description(fop_stat));
			return NULL;
		}

		int stat;
		stat = luaL_loadstring( st, const_cast<const char*>((char*)buffer) );

		if( stat == LUA_OK ) { // was it loaded successfully?
            //kprintf("Performing call..\n");
            stat = lua_pcall( st, 0, 0, 0 );
            if( stat == LUA_OK ) { // get return values
            	kprintf("CC state initialized successfully.\n");
            }
        } else { // was there an error?
            const char *err = lua_tostring(st, -1);
            kprintf("lua-error: %s\n", err);
            lua_pop(st, 1);
            lua_close(st);

            return NULL;
        }

		cc_process* proc = new cc_process;

		proc->state = st;

		channel_receiver *tmp = new channel_receiver( listen_to_channel("timer") );
		proc->events.add_end(tmp);

		tmp = new channel_receiver( listen_to_channel("keypress") );
		proc->events.add_end(tmp);

		return proc;
	}

	void load_file( cc_process* proc, unsigned char* file ) {
		vfs_node* node = NULL;
		vfs::vfs_status fop_stat = vfs::get_file_info( file, &node );
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("cc-load_file: FS get info failed with error: %s\n", vfs::status_description(fop_stat));
			return;
		}

		if( node != NULL ) {
			if( node->type != vfs_node_types::file ) {
				kprintf("cc-load_file: FS read failed with error: incorrect type\n");
				return;
			}
		}

		vfs_file* fn = (vfs_file*)node;
		void* buffer = kmalloc( fn->size );

		fop_stat = vfs::read_file( file, buffer );
		if( fop_stat != vfs::vfs_status::ok ) {
			kprintf("cc-load_file: FS read failed with error: %s\n", vfs::status_description(fop_stat));
			return;
		}

		int stat;
		stat = luaL_loadstring( proc->state, const_cast<const char*>((char*)buffer) );

		if( stat != LUA_OK ) { // was there an error?
			const char *err = lua_tostring(proc->state, -1);
			kprintf("lua-error: %s\n", err);
			lua_pop(proc->state, 1);
			lua_close(proc->state);
		}
	}

	void event_loop( cc_process* proc ) {
		int routine_stat = lua_resume( proc->state, NULL, 0 );
		while(true) {
			if( routine_stat != LUA_YIELD ) {
				break;
			}

			message* m = NULL;
			char* type = NULL;
			if( lua_isstring( proc->state, -1 ) ) {
				const char *ev = lua_tostring( proc->state, -1 );

				if( strcmp( const_cast<char*>(ev), const_cast<char*>("key") ) || strcmp( const_cast<char*>(ev), const_cast<char*>("char") ) ) {
					ev = "keypress"; // no deletion for meeeeeeee
				}

				channel_receiver recv = listen_to_channel(const_cast<char*>(ev));

				recv.wait();

				m = recv.queue.remove(0);
				type = const_cast<char*>(ev);
			} else {
				unsigned int n = wait_multiple( proc->events );

				m = proc->events[n]->queue.remove(0);

				if( n == 0 ) { // TODO: find a better way to do this
					type = const_cast<char*>("timer");
				} else if( n == 1) {
					type = const_cast<char*>("keypress");
				}
			}

			if( type == NULL ) {
				lua_pushnil( proc->state );
				lua_pushstring( proc->state, "event not supported" );

				routine_stat = lua_resume( proc->state, NULL, 2 );
				continue;
			}

			if( strcmp( type, "keypress" ) ) {
				ps2_keypress* d = (ps2_keypress*)m->data;

				if( d->is_ascii && (d->character == 'T') && d->ctrl ) {
					lua_pushstring( proc->state, "terminate" );
					delete m;
					routine_stat = lua_resume( proc->state, NULL, 1 );
					continue;
				}

				lua_pushstring( proc->state, "key" );
				lua_pushnumber( proc->state, d->key );
				lua_pushboolean( proc->state, d->released );
				lua_pushboolean( proc->state, d->shift );
				lua_pushboolean( proc->state, d->ctrl );
				lua_pushboolean( proc->state, d->alt );

				if( d->is_ascii ) {
					lua_pushboolean( proc->state, d->character );

					routine_stat = lua_resume( proc->state, NULL, 7 );
				} else {
					routine_stat = lua_resume( proc->state, NULL, 6 );
				}

				delete m;
			} else if( strcmp( type, "timer" ) ) {
				timer* t = (timer*)m->data;

				lua_pushstring( proc->state, "timer" );
				lua_pushnumber( proc->state, t->id );

				delete m;

				routine_stat = lua_resume( proc->state, NULL, 2 );
				// message destructor frees data
			} else {
				lua_pushnil( proc->state );
				lua_pushstring( proc->state, "event not supported" );

				routine_stat = lua_resume( proc->state, NULL, 2 );
			}
		}
	}

}


