#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "config.h"
#include <stdlib.h>

int argc;
char** argv;

int main(int _argc, char** _argv)
{
    argc = _argc;
    argv = _argv;

#ifdef LUASEARCHPATH
    // Only set LUA_PATH if its not already defined on the environment.
    setenv("LUA_PATH", LUASEARCHPATH, /* overwrite */ 0);
#endif

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    return luaL_dostring(L, "require \"entrypoint\"");
}
