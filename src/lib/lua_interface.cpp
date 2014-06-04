#include "includes.h"
extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}
#include "core/scheduler.h"

int lua_process_wrapper( void* program ) {
    const char* prog = (const char*)program;
    lua_State* st = luaL_newstate();
    int retval = 0;
    
    luaL_openlibs(st);
    // load C functions here
    int stat = luaL_loadstring( st, prog );
    
    if( stat == 0 ) { // was it loaded successfully?
        stat = lua_pcall( st, 0, 1, 0 );
        if( stat == 0 ) { // get return values
            retval = lua_tonumber(st, -1);
            lua_pop(st, 1);
        }
    }
    if( stat != 0 ) { // was there an error?
        const char *err = lua_tostring(st, -1);
        kprintf("Lua error in process %u:", process_current->id);
        kprintf("%s\n", err);
        lua_pop(st, 1);
        retval = stat;
    }
    lua_close(st);
    return retval;
}

process *spawn_new_lua_process( const char* program, bool is_usermode, int priority, const char* name ) {
    return new process( (size_t)&lua_process_wrapper, is_usermode, priority, name, (void*)program, 1 );
}