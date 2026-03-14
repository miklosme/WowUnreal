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

static const char* WOW_CONTEXT_KEY = "WowLuaContext";

const char* WowLuaApi::FRAME_METATABLE = "WowFrame";

void WowLuaApi::RegisterAll(lua_State* L)
{
#if HAS_LUA
    if (!L) return;
    RegisterGlobals(L);
    RegisterStubs(L);
    RegisterFrameApi(L);
#endif
}

void WowLuaApi::SetContext(lua_State* L, FWowLuaContext* Ctx)
{
#if HAS_LUA
    if (!L) return;
    lua_pushlightuserdata(L, Ctx);
    lua_setfield(L, LUA_REGISTRYINDEX, WOW_CONTEXT_KEY);
#endif
}

FWowLuaContext* WowLuaApi::GetContext(lua_State* L)
{
#if HAS_LUA
    if (!L) return nullptr;
    lua_getfield(L, LUA_REGISTRYINDEX, WOW_CONTEXT_KEY);
    FWowLuaContext* Ctx = static_cast<FWowLuaContext*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return Ctx;
#else
    return nullptr;
#endif
}
