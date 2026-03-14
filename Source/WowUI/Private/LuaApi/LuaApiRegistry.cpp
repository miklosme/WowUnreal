#include "LuaApiRegistry.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#define HAS_LUA 1
#else
#define HAS_LUA 0
#endif

void WowLuaApi::RegisterAll(lua_State* L)
{
#if HAS_LUA
    if (!L) return;
    RegisterGlobals(L);
    RegisterStubs(L);
#endif
}
