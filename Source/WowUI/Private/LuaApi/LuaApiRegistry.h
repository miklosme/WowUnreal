#pragma once

struct lua_State;
class FWowEntityManager;
class UWowConnectionManager;
class FWowEventSystem;
class FWowFrameManager;

/** Context passed to Lua API functions so they can access game state */
struct FWowLuaContext
{
    FWowEntityManager* EntityManager = nullptr;
    UWowConnectionManager* ConnectionManager = nullptr;
    FWowEventSystem* EventSystem = nullptr;
    FWowFrameManager* FrameManager = nullptr;
};

namespace WowLuaApi
{
    void RegisterAll(lua_State* L);
    void RegisterGlobals(lua_State* L);
    void RegisterStubs(lua_State* L);
    void RegisterFrameApi(lua_State* L);

    /** Store game context pointer in Lua registry for API functions to use */
    void SetContext(lua_State* L, FWowLuaContext* Ctx);

    /** Retrieve context from Lua registry (returns nullptr if not set) */
    FWowLuaContext* GetContext(lua_State* L);

    /** Key for the frame metatable in the Lua registry */
    extern const char* FRAME_METATABLE;
}
