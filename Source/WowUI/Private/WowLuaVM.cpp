#include "WowLuaVM.h"
#include "LuaApi/LuaApiRegistry.h"
#include "WowSavedVariables.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowLua, Log, All);

// Lua headers - conditionally include
#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#define HAS_LUA 1
#else
#define HAS_LUA 0
typedef struct lua_State lua_State;
#endif

FWowLuaVM::FWowLuaVM() {}
FWowLuaVM::~FWowLuaVM() { Shutdown(); }

bool FWowLuaVM::Initialize()
{
#if HAS_LUA
    if (L) Shutdown();
    L = luaL_newstate();
    if (!L) return false;
    luaL_openlibs(L);
    SandboxGlobals();
    RegisterWowApi();
    UE_LOG(LogWowLua, Log, TEXT("Lua 5.1 VM initialized"));
    return true;
#else
    UE_LOG(LogWowLua, Warning, TEXT("Lua not available"));
    return false;
#endif
}

void FWowLuaVM::Shutdown()
{
#if HAS_LUA
    if (L)
    {
        FWowSavedVariables::SaveAll(L);
        lua_close(L);
        L = nullptr;
    }
#endif
}

void FWowLuaVM::SaveAllVariables()
{
#if HAS_LUA
    if (L) FWowSavedVariables::SaveAll(L);
#endif
}

void FWowLuaVM::SandboxGlobals()
{
#if HAS_LUA
    if (!L) return;
    for (const char* n : {"io","os","debug","loadfile","dofile","require","package"})
    { lua_pushnil(L); lua_setglobal(L, n); }
#endif
}

void FWowLuaVM::RegisterWowApi()
{
#if HAS_LUA
    if (!L) return;
    WowLuaApi::RegisterAll(L);
    UE_LOG(LogWowLua, Log, TEXT("WoW Lua API registered"));
#endif
}

bool FWowLuaVM::ExecuteString(const FString& Code, const FString& ChunkName)
{
#if HAS_LUA
    if (!L) return false;
    FTCHARToUTF8 C(*Code); FTCHARToUTF8 N(*ChunkName);
    if (luaL_loadbuffer(L, C.Get(), C.Length(), N.Get()) != 0)
    { UE_LOG(LogWowLua, Error, TEXT("Lua load: %s"), UTF8_TO_TCHAR(lua_tostring(L,-1))); lua_pop(L,1); return false; }
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0)
    { UE_LOG(LogWowLua, Error, TEXT("Lua exec: %s"), UTF8_TO_TCHAR(lua_tostring(L,-1))); lua_pop(L,1); return false; }
    return true;
#else
    return false;
#endif
}

bool FWowLuaVM::ExecuteBuffer(const TArray<uint8>& Buffer, const FString& ChunkName)
{
#if HAS_LUA
    if (!L) return false;
    FTCHARToUTF8 N(*ChunkName);
    if (luaL_loadbuffer(L, (const char*)Buffer.GetData(), Buffer.Num(), N.Get()) != 0)
    { UE_LOG(LogWowLua, Error, TEXT("Lua load %s: %s"), *ChunkName, UTF8_TO_TCHAR(lua_tostring(L,-1))); lua_pop(L,1); return false; }
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0)
    { UE_LOG(LogWowLua, Error, TEXT("Lua %s: %s"), *ChunkName, UTF8_TO_TCHAR(lua_tostring(L,-1))); lua_pop(L,1); return false; }
    return true;
#else
    return false;
#endif
}

void FWowLuaVM::FireEvent(const FString& Name, const TArray<FString>& Args)
{
    UE_LOG(LogWowLua, Verbose, TEXT("Event: %s (%d args)"), *Name, Args.Num());
}
