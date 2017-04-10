#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

int argc;
char** argv;

int main(int _argc, char** _argv)
{
    argc = _argc;
    argv = _argv;

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    return luaL_dostring(L, "require \"entrypoint\"");
}
