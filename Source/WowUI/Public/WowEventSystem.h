#pragma once
#include "CoreMinimal.h"

struct lua_State;
class FWowLuaVM;
class FWowFrameManager;
struct FWowFrameDef;

/**
 * Maps SMSG opcodes to WoW event names and dispatches events to registered frames.
 */
class WOWUI_API FWowEventSystem
{
public:
	/** Set the Lua VM and frame manager for event dispatch */
	void SetLuaVM(FWowLuaVM* InVM) { LuaVM = InVM; }
	void SetFrameManager(FWowFrameManager* InMgr) { FrameManager = InMgr; }

	/** Register a frame to receive an event */
	void RegisterEvent(int64 FrameHandle, const FString& EventName);

	/** Unregister a frame from an event */
	void UnregisterEvent(int64 FrameHandle, const FString& EventName);

	/** Unregister a frame from all events */
	void UnregisterAllEvents(int64 FrameHandle);

	/** Fire an event to all registered frames, calling their OnEvent Lua handlers */
	void FireEvent(const FString& EventName, const TArray<FString>& Args = {});

	/** Compile a script handler for a frame and store the Lua function reference */
	void SetFrameScript(int64 Handle, const FString& ScriptName, const FString& Code);

	/** Create a Lua table representing a frame (sets it as a named global if the frame has a name) */
	void CreateFrameObject(int64 Handle, const FString& FrameName);

	/** Remove all compiled scripts and the Lua frame object for a handle */
	void RemoveFrameScripts(int64 Handle);

	/** Compile all scripts from a frame definition */
	void CompileFrameScripts(int64 Handle, const FWowFrameDef& Def);

	/** Map a server opcode to a WoW event name */
	static FString OpcodeToEvent(uint16 Opcode);

private:
	FWowLuaVM* LuaVM = nullptr;
	FWowFrameManager* FrameManager = nullptr;

	/** Event name -> set of registered frame handles */
	TMap<FString, TSet<int64>> EventRegistrations;

	/** Compiled script Lua references: Handle -> {ScriptName -> LuaRef} */
	TMap<int64, TMap<FString, int32>> ScriptRefs;

	/** Lua frame object references: Handle -> LuaRef */
	TMap<int64, int32> FrameObjectRefs;
};
