#include "WowLuaVM.h"
#include "LuaApi/LuaApiRegistry.h"
#include "WowSavedVariables.h"
#include "WowEventSystem.h"

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

// ── Memory Allocator ─────────────────────────────────────────────────────────────

void* FWowLuaVM::LuaAlloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	FWowLuaVM* VM = static_cast<FWowLuaVM*>(ud);

	if (nsize == 0)
	{
		// Free
		if (ptr)
		{
			VM->MemoryUsed -= osize;
			FMemory::Free(ptr);
		}
		return nullptr;
	}

	// Check memory limit before allocating
	SIZE_T Delta = nsize - (ptr ? osize : 0);
	if (VM->MemoryLimit > 0 && VM->MemoryUsed + Delta > VM->MemoryLimit)
	{
		UE_LOG(LogWowLua, Error, TEXT("Lua memory limit exceeded (%zu / %zu bytes)"),
			VM->MemoryUsed + Delta, VM->MemoryLimit);
		return nullptr; // Allocation denied — triggers Lua out-of-memory error
	}

	void* NewPtr = FMemory::Realloc(ptr, nsize);
	if (NewPtr)
	{
		VM->MemoryUsed += Delta;
	}
	return NewPtr;
}

// ── Instruction Hook ─────────────────────────────────────────────────────────────

#if HAS_LUA
static const char* INSTRUCTION_COUNT_KEY = "__WowInstructionCount";

static void WowLuaInstructionHook(lua_State* LS, lua_Debug* ar)
{
	// Get the VM instance from the registry
	lua_getfield(LS, LUA_REGISTRYINDEX, "__WowLuaVM");
	FWowLuaVM* VM = static_cast<FWowLuaVM*>(lua_touserdata(LS, -1));
	lua_pop(LS, 1);

	if (!VM || VM->GetInstructionLimit() <= 0) return;

	// Get and increment instruction counter
	lua_getfield(LS, LUA_REGISTRYINDEX, INSTRUCTION_COUNT_KEY);
	int32 Count = static_cast<int32>(lua_tointeger(LS, -1)) + 1000; // hook fires every 1000 instructions
	lua_pop(LS, 1);

	if (Count > VM->GetInstructionLimit())
	{
		luaL_error(LS, "Script execution exceeded instruction limit (%d)", VM->GetInstructionLimit());
	}

	lua_pushinteger(LS, Count);
	lua_setfield(LS, LUA_REGISTRYINDEX, INSTRUCTION_COUNT_KEY);
}
#endif

void FWowLuaVM::InstallInstructionHook()
{
#if HAS_LUA
	if (!L || InstructionLimit <= 0) return;

	// Store VM pointer in registry for the hook callback
	lua_pushlightuserdata(L, this);
	lua_setfield(L, LUA_REGISTRYINDEX, "__WowLuaVM");

	// Initialize counter
	lua_pushinteger(L, 0);
	lua_setfield(L, LUA_REGISTRYINDEX, INSTRUCTION_COUNT_KEY);

	// Set hook: fire every 1000 instructions
	lua_sethook(L, WowLuaInstructionHook, LUA_MASKCOUNT, 1000);

	UE_LOG(LogWowLua, Log, TEXT("Instruction hook installed (limit: %d)"), InstructionLimit);
#endif
}

// ── VM Lifecycle ─────────────────────────────────────────────────────────────────

bool FWowLuaVM::Initialize()
{
#if HAS_LUA
	if (L) Shutdown();

	// Create state with custom memory allocator
	L = lua_newstate(LuaAlloc, this);
	if (!L) return false;

	MemoryUsed = 0; // Reset counter

	luaL_openlibs(L);
	SandboxGlobals();
	RegisterWowApi();
	InstallInstructionHook();

	UE_LOG(LogWowLua, Log, TEXT("Lua 5.1 VM initialized (memory limit: %zu MB, instruction limit: %d)"),
		MemoryLimit / (1024 * 1024), InstructionLimit);
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
		MemoryUsed = 0;
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

	// Save safe os functions before removing the module
	// WoW FrameXML needs: date(), time(), difftime(), clock()
	lua_getglobal(L, "os");
	if (lua_istable(L, -1))
	{
		// Save os.time, os.date, os.difftime, os.clock as globals
		lua_getfield(L, -1, "time");
		lua_setglobal(L, "time");

		lua_getfield(L, -1, "date");
		lua_setglobal(L, "date");

		lua_getfield(L, -1, "difftime");
		lua_setglobal(L, "difftime");

		lua_getfield(L, -1, "clock");
		lua_setglobal(L, "clock");
	}
	lua_pop(L, 1);

	// Save debug.traceback before removing debug module
	lua_getglobal(L, "debug");
	if (lua_istable(L, -1))
	{
		lua_getfield(L, -1, "traceback");
		lua_setglobal(L, "debugstack"); // WoW uses debugstack() instead of debug.traceback()
	}
	lua_pop(L, 1);

	// Remove dangerous modules
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

	// Reset instruction counter for this execution
	lua_pushinteger(L, 0);
	lua_setfield(L, LUA_REGISTRYINDEX, INSTRUCTION_COUNT_KEY);

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

	// Reset instruction counter for this execution
	lua_pushinteger(L, 0);
	lua_setfield(L, LUA_REGISTRYINDEX, INSTRUCTION_COUNT_KEY);

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

#if HAS_LUA
	if (L)
	{
		// Get the EventSystem from the Lua context to dispatch the event
		FWowLuaContext* Context = WowLuaApi::GetContext(L);
		if (Context && Context->EventSystem)
		{
			Context->EventSystem->FireEvent(Name, Args);
		}
		else
		{
			UE_LOG(LogWowLua, Warning, TEXT("Cannot fire event '%s': EventSystem not available in Lua context"), *Name);
		}
	}
#endif
}
