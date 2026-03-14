#pragma once

struct lua_State;
class FWowEntityManager;
class UWowConnectionManager;

/** Context passed to Lua API functions so they can access game state */
struct FWowLuaContext
{
    FWowEntityManager* EntityManager = nullptr;
    UWowConnectionManager* ConnectionManager = nullptr;
};

namespace WowLuaApi
{
    void RegisterAll(lua_State* L);
    void RegisterGlobals(lua_State* L);
    void RegisterStubs(lua_State* L);

    /** Store game context pointer in Lua registry for API functions to use */
    void SetContext(lua_State* L, FWowLuaContext* Ctx);

    /** Retrieve context from Lua registry (returns nullptr if not set) */
    FWowLuaContext* GetContext(lua_State* L);
}
