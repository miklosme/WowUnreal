#pragma once

struct lua_State;

namespace WowLuaApi
{
    void RegisterAll(lua_State* L);
    void RegisterGlobals(lua_State* L);
    void RegisterStubs(lua_State* L);
}
