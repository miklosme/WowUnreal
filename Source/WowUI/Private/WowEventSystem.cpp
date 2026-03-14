#include "WowEventSystem.h"
#include "WowLuaVM.h"
#include "WowFrameTypes.h"
#include "LuaApi/LuaApiRegistry.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogWowEvent, Log, All);

void FWowEventSystem::RegisterEvent(int64 FrameHandle, const FString& EventName)
{
	EventRegistrations.FindOrAdd(EventName).Add(FrameHandle);
	UE_LOG(LogWowEvent, Verbose, TEXT("Frame %lld registered for event: %s"), FrameHandle, *EventName);
}

void FWowEventSystem::UnregisterEvent(int64 FrameHandle, const FString& EventName)
{
	if (TSet<int64>* Handles = EventRegistrations.Find(EventName))
	{
		Handles->Remove(FrameHandle);
		if (Handles->Num() == 0)
		{
			EventRegistrations.Remove(EventName);
		}
	}
}

void FWowEventSystem::UnregisterAllEvents(int64 FrameHandle)
{
	TArray<FString> EmptyKeys;
	for (auto& Pair : EventRegistrations)
	{
		Pair.Value.Remove(FrameHandle);
		if (Pair.Value.Num() == 0)
		{
			EmptyKeys.Add(Pair.Key);
		}
	}
	for (const FString& Key : EmptyKeys)
	{
		EventRegistrations.Remove(Key);
	}
}

void FWowEventSystem::FireEvent(const FString& EventName, const TArray<FString>& Args)
{
	const TSet<int64>* Handles = EventRegistrations.Find(EventName);
	if (!Handles || Handles->Num() == 0)
	{
		return;
	}

	UE_LOG(LogWowEvent, Verbose, TEXT("Firing event %s to %d frames"), *EventName, Handles->Num());

#if HAS_LUA
	if (!LuaVM || !LuaVM->IsInitialized())
	{
		return;
	}

	lua_State* L = LuaVM->GetState();
	if (!L)
	{
		return;
	}

	// Copy handles — handlers may modify the registration set during iteration
	TArray<int64> HandlesCopy = Handles->Array();

	for (int64 Handle : HandlesCopy)
	{
		// Look up compiled OnEvent function
		TMap<FString, int32>* FrameScripts = ScriptRefs.Find(Handle);
		if (!FrameScripts) continue;

		int32* ScriptRef = FrameScripts->Find(TEXT("OnEvent"));
		if (!ScriptRef) continue;

		// Push the compiled OnEvent function
		lua_rawgeti(L, LUA_REGISTRYINDEX, *ScriptRef);
		if (!lua_isfunction(L, -1))
		{
			lua_pop(L, 1);
			continue;
		}

		// Push self (frame table)
		int32* FrameRef = FrameObjectRefs.Find(Handle);
		if (FrameRef)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, *FrameRef);
		}
		else
		{
			lua_newtable(L); // fallback empty table
		}

		// Push event name
		FTCHARToUTF8 UTF8Event(*EventName);
		lua_pushstring(L, UTF8Event.Get());

		// Push args
		for (const FString& Arg : Args)
		{
			FTCHARToUTF8 UTF8Arg(*Arg);
			lua_pushstring(L, UTF8Arg.Get());
		}

		// Call: func(self, event, arg1, arg2, ...)
		int32 NumArgs = 2 + Args.Num();
		if (lua_pcall(L, NumArgs, 0, 0) != 0)
		{
			UE_LOG(LogWowEvent, Error, TEXT("OnEvent error [frame %lld] %s: %s"),
				Handle, *EventName, UTF8_TO_TCHAR(lua_tostring(L, -1)));
			lua_pop(L, 1);
		}
	}
#endif
}

void FWowEventSystem::SetFrameScript(int64 Handle, const FString& ScriptName, const FString& Code)
{
#if HAS_LUA
	if (!LuaVM || Code.IsEmpty()) return;

	lua_State* L = LuaVM->GetState();
	if (!L) return;

	// Compile: function(self, event, ...) <code> end
	FString Wrapped = FString::Printf(TEXT("return function(self, event, ...) %s end"), *Code);
	FTCHARToUTF8 UTF8Code(*Wrapped);
	FString ChunkName = FString::Printf(TEXT("=[%lld]:%s"), Handle, *ScriptName);
	FTCHARToUTF8 UTF8Chunk(*ChunkName);

	if (luaL_loadbuffer(L, UTF8Code.Get(), UTF8Code.Length(), UTF8Chunk.Get()) != 0)
	{
		UE_LOG(LogWowEvent, Error, TEXT("Script compile error [%lld] %s: %s"),
			Handle, *ScriptName, UTF8_TO_TCHAR(lua_tostring(L, -1)));
		lua_pop(L, 1);
		return;
	}

	// Execute the chunk to get the function on the stack
	if (lua_pcall(L, 0, 1, 0) != 0)
	{
		UE_LOG(LogWowEvent, Error, TEXT("Script load error [%lld] %s: %s"),
			Handle, *ScriptName, UTF8_TO_TCHAR(lua_tostring(L, -1)));
		lua_pop(L, 1);
		return;
	}

	// Store the function as a registry reference
	int32 Ref = luaL_ref(L, LUA_REGISTRYINDEX);

	// Remove old ref if any
	TMap<FString, int32>& FrameScripts = ScriptRefs.FindOrAdd(Handle);
	if (int32* OldRef = FrameScripts.Find(ScriptName))
	{
		luaL_unref(L, LUA_REGISTRYINDEX, *OldRef);
	}
	FrameScripts.Add(ScriptName, Ref);

	// Track frames with OnUpdate handlers for tick dispatch
	if (ScriptName == TEXT("OnUpdate"))
	{
		OnUpdateFrames.Add(Handle);
	}

	UE_LOG(LogWowEvent, Verbose, TEXT("Compiled script [%lld] %s (ref %d)"), Handle, *ScriptName, Ref);
#endif
}

void FWowEventSystem::CreateFrameObject(int64 Handle, const FString& FrameName)
{
#if HAS_LUA
	if (!LuaVM) return;

	lua_State* L = LuaVM->GetState();
	if (!L) return;

	// Create a Lua table to represent this frame
	lua_newtable(L);

	// Store the handle
	lua_pushinteger(L, Handle);
	lua_setfield(L, -2, "__handle");

	// Store the name
	if (!FrameName.IsEmpty())
	{
		FTCHARToUTF8 UTF8Name(*FrameName);
		lua_pushstring(L, UTF8Name.Get());
		lua_setfield(L, -2, "__name");
	}

	// Set the frame metatable so frame:Method() calls work
	luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
	if (!lua_isnil(L, -1))
	{
		lua_setmetatable(L, -2);
	}
	else
	{
		lua_pop(L, 1); // pop nil if metatable not registered yet
	}

	// Store as registry ref
	int32 Ref = luaL_ref(L, LUA_REGISTRYINDEX);

	// Clean up old ref
	if (int32* OldRef = FrameObjectRefs.Find(Handle))
	{
		luaL_unref(L, LUA_REGISTRYINDEX, *OldRef);
	}
	FrameObjectRefs.Add(Handle, Ref);

	// Also register as a global variable if the frame has a name
	if (!FrameName.IsEmpty())
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
		FTCHARToUTF8 UTF8Name(*FrameName);
		lua_setglobal(L, UTF8Name.Get());
	}

	UE_LOG(LogWowEvent, Verbose, TEXT("Created frame object [%lld] %s"), Handle, *FrameName);
#endif
}

void FWowEventSystem::RemoveFrameScripts(int64 Handle)
{
#if HAS_LUA
	if (!LuaVM) return;
	lua_State* L = LuaVM->GetState();
	if (!L) return;

	// Remove script refs
	if (TMap<FString, int32>* FrameScripts = ScriptRefs.Find(Handle))
	{
		for (auto& Pair : *FrameScripts)
		{
			luaL_unref(L, LUA_REGISTRYINDEX, Pair.Value);
		}
		ScriptRefs.Remove(Handle);
	}

	// Remove from OnUpdate tracking
	OnUpdateFrames.Remove(Handle);

	// Remove frame object ref
	if (int32* FrameRef = FrameObjectRefs.Find(Handle))
	{
		luaL_unref(L, LUA_REGISTRYINDEX, *FrameRef);
		FrameObjectRefs.Remove(Handle);
	}
#endif
}

void FWowEventSystem::CompileFrameScripts(int64 Handle, const FWowFrameDef& Def)
{
	// Create the Lua frame object first
	CreateFrameObject(Handle, Def.Name);

	// Compile each script handler from the frame definition
	for (const FWowScriptHandler& Script : Def.Scripts)
	{
		if (!Script.Code.IsEmpty())
		{
			SetFrameScript(Handle, Script.Event, Script.Code);
		}
	}

	UE_LOG(LogWowEvent, Verbose, TEXT("Compiled %d scripts for frame [%lld] %s"),
		Def.Scripts.Num(), Handle, *Def.Name);
}

void FWowEventSystem::TickOnUpdate(float DeltaTime)
{
#if HAS_LUA
	if (!LuaVM || !LuaVM->IsInitialized() || OnUpdateFrames.Num() == 0)
	{
		return;
	}

	lua_State* L = LuaVM->GetState();
	if (!L) return;

	TArray<int64> Handles = OnUpdateFrames.Array();

	for (int64 Handle : Handles)
	{
		TMap<FString, int32>* FrameScripts = ScriptRefs.Find(Handle);
		if (!FrameScripts) continue;

		int32* ScriptRef = FrameScripts->Find(TEXT("OnUpdate"));
		if (!ScriptRef) continue;

		// Push the compiled OnUpdate function
		lua_rawgeti(L, LUA_REGISTRYINDEX, *ScriptRef);
		if (!lua_isfunction(L, -1))
		{
			lua_pop(L, 1);
			continue;
		}

		// Push self (frame table)
		int32* FrameRef = FrameObjectRefs.Find(Handle);
		if (FrameRef)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, *FrameRef);
		}
		else
		{
			lua_newtable(L);
		}

		// Push elapsed time
		lua_pushnumber(L, static_cast<double>(DeltaTime));

		// Call: func(self, elapsed)
		if (lua_pcall(L, 2, 0, 0) != 0)
		{
			UE_LOG(LogWowEvent, Error, TEXT("OnUpdate error [frame %lld]: %s"),
				Handle, UTF8_TO_TCHAR(lua_tostring(L, -1)));
			lua_pop(L, 1);
		}
	}
#endif
}

FString FWowEventSystem::OpcodeToEvent(uint16 Opcode)
{
	// WoW 3.3.5 (build 12340) SMSG opcodes mapped to UI event names.
	switch (Opcode)
	{
	// Character / login
	case 0x0236: return TEXT("PLAYER_LOGIN");                    // SMSG_LOGIN_VERIFY_WORLD
	case 0x0069: return TEXT("PLAYER_ENTERING_WORLD");           // SMSG_UPDATE_OBJECT (initial)
	case 0x006E: return TEXT("PLAYER_LEAVING_WORLD");            // SMSG_LOGOUT_COMPLETE

	// Targeting
	case 0x0078: return TEXT("PLAYER_TARGET_CHANGED");           // MSG_RAID_TARGET_UPDATE (contextual)

	// Unit updates
	case 0x02A2: return TEXT("UNIT_HEALTH");                     // SMSG_HEALTH_UPDATE
	case 0x02A3: return TEXT("UNIT_MANA");                       // SMSG_POWER_UPDATE
	case 0x0496: return TEXT("UNIT_AURA");                       // SMSG_AURA_UPDATE
	case 0x0089: return TEXT("UNIT_NAME_UPDATE");                // SMSG_NAME_QUERY_RESPONSE

	// Chat messages
	case 0x0096: return TEXT("CHAT_MSG_SAY");                    // SMSG_MESSAGECHAT (filtered by type)
	case 0x0097: return TEXT("CHAT_MSG_WHISPER");                // SMSG_MESSAGECHAT
	case 0x0098: return TEXT("CHAT_MSG_PARTY");                  // SMSG_MESSAGECHAT
	case 0x0099: return TEXT("CHAT_MSG_GUILD");                  // SMSG_MESSAGECHAT
	case 0x009A: return TEXT("CHAT_MSG_YELL");                   // SMSG_MESSAGECHAT

	// Spells and actions
	case 0x012C: return TEXT("SPELLS_CHANGED");                  // SMSG_INITIAL_SPELLS / SMSG_LEARNED_SPELL
	case 0x012D: return TEXT("SPELL_UPDATE_COOLDOWN");           // SMSG_SPELL_COOLDOWN
	case 0x0140: return TEXT("ACTIONBAR_SLOT_CHANGED");          // SMSG_UPDATE_ACTION_BUTTONS

	// Inventory
	case 0x0134: return TEXT("BAG_UPDATE");                      // SMSG_INVENTORY_CHANGE_FAILURE / UPDATE
	case 0x0166: return TEXT("ITEM_LOCK_CHANGED");

	// Quest
	case 0x019B: return TEXT("QUEST_LOG_UPDATE");                // SMSG_QUESTLOG_FULL_UPDATE
	case 0x019C: return TEXT("QUEST_FINISHED");

	// Zone / map
	case 0x02C2: return TEXT("ZONE_CHANGED_NEW_AREA");           // SMSG_ZONE_UNDER_ATTACK / area update
	case 0x02C3: return TEXT("ZONE_CHANGED");
	case 0x02C4: return TEXT("ZONE_CHANGED_INDOORS");

	// Group / raid
	case 0x007D: return TEXT("GROUP_ROSTER_UPDATE");             // SMSG_GROUP_LIST
	case 0x007E: return TEXT("RAID_ROSTER_UPDATE");              // SMSG_RAID_GROUP_ONLY

	// Combat
	case 0x04D2: return TEXT("COMBAT_LOG_EVENT_UNFILTERED");     // SMSG_COMBAT_LOG_MULTIPLE

	// Error / notifications
	case 0x0060: return TEXT("UI_ERROR_MESSAGE");                // SMSG_INVENTORY_CHANGE_FAILURE (contextual)
	case 0x0061: return TEXT("UI_INFO_MESSAGE");

	// Trade / economy
	case 0x010E: return TEXT("TRADE_SHOW");
	case 0x010F: return TEXT("TRADE_CLOSED");
	case 0x0257: return TEXT("PLAYER_MONEY");

	// Guild
	case 0x0054: return TEXT("GUILD_ROSTER_UPDATE");             // SMSG_GUILD_ROSTER
	case 0x0091: return TEXT("GUILD_MOTD");

	// Loot
	case 0x0160: return TEXT("LOOT_OPENED");                     // SMSG_LOOT_RESPONSE
	case 0x0161: return TEXT("LOOT_CLOSED");                     // SMSG_LOOT_RELEASE_RESPONSE

	// Mail
	case 0x023C: return TEXT("MAIL_INBOX_UPDATE");               // SMSG_MAIL_LIST_RESULT

	// Talent / skills
	case 0x012F: return TEXT("CHARACTER_POINTS_CHANGED");
	case 0x0462: return TEXT("PLAYER_TALENT_UPDATE");            // SMSG_TALENTS_INFO
	case 0x0130: return TEXT("SKILL_LINES_CHANGED");

	// Merchant / vendor
	case 0x019F: return TEXT("MERCHANT_SHOW");                   // SMSG_LIST_INVENTORY
	case 0x01B4: return TEXT("MERCHANT_CLOSED");

	// Friend list
	case 0x0068: return TEXT("FRIENDLIST_UPDATE");               // SMSG_FRIEND_LIST

	// Battleground / PvP
	case 0x02E8: return TEXT("UPDATE_BATTLEFIELD_STATUS");       // SMSG_BATTLEFIELD_STATUS
	case 0x02EC: return TEXT("BATTLEFIELDS_SHOW");

	// Achievement
	case 0x0468: return TEXT("ACHIEVEMENT_EARNED");              // SMSG_ACHIEVEMENT_EARNED

	// Death / resurrection
	case 0x02E7: return TEXT("PLAYER_DEAD");
	case 0x0163: return TEXT("PLAYER_ALIVE");
	case 0x015E: return TEXT("CORPSE_IN_RANGE");

	default:
		return FString();
	}
}
