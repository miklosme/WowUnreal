#include "WowEventSystem.h"
#include "WowLuaVM.h"
#include "WowFrameTypes.h"
#include "WowFrameManager.h"
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
		// Log when important events have no listeners
		if (EventName == TEXT("ACTIONBAR_SLOT_CHANGED") || EventName == TEXT("ACTIONBAR_UPDATE_STATE") ||
		    EventName == TEXT("UNIT_HEALTH") || EventName == TEXT("UNIT_MANA"))
		{
			UE_LOG(LogWowEvent, Warning, TEXT("Event %s has NO registered frames"), *EventName);
		}
		return;
	}

	UE_LOG(LogWowEvent, Log, TEXT("Firing event %s to %d frames (args: %d)"), *EventName, Handles->Num(), Args.Num());

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

	// Wrap the script code with the correct function signature for each script type.
	// WoW scripts have different parameter names depending on their type:
	//   OnEvent:  function(self, event, ...)
	//   OnUpdate: function(self, elapsed)
	//   OnClick:  function(self, button, down)
	//   OnShow/OnHide/OnLoad: function(self)
	//   OnEnter/OnLeave: function(self, motion)
	//   OnMouseDown/Up: function(self, button)
	//   OnValueChanged: function(self, value)
	//   OnTextChanged: function(self)
	//   OnChar: function(self, text)
	//   OnSizeChanged: function(self, w, h)
	//   OnScrollRangeChanged: function(self, xrange, yrange)
	// All WoW script handlers include ... for forward compatibility.
	// FrameXML code frequently uses ... even in handlers that don't formally have varargs.
	FString Params;
	if (ScriptName == TEXT("OnEvent"))
		Params = TEXT("self, event, ...");
	else if (ScriptName == TEXT("OnUpdate"))
		Params = TEXT("self, elapsed, ...");
	else if (ScriptName == TEXT("OnClick"))
		Params = TEXT("self, button, down, ...");
	else if (ScriptName == TEXT("OnEnter") || ScriptName == TEXT("OnLeave"))
		Params = TEXT("self, motion, ...");
	else if (ScriptName == TEXT("OnMouseDown") || ScriptName == TEXT("OnMouseUp"))
		Params = TEXT("self, button, ...");
	else if (ScriptName == TEXT("OnValueChanged"))
		Params = TEXT("self, value, ...");
	else if (ScriptName == TEXT("OnChar"))
		Params = TEXT("self, text, ...");
	else if (ScriptName == TEXT("OnSizeChanged"))
		Params = TEXT("self, w, h, ...");
	else if (ScriptName == TEXT("OnScrollRangeChanged"))
		Params = TEXT("self, xrange, yrange, ...");
	else if (ScriptName == TEXT("OnDragStart") || ScriptName == TEXT("OnDragStop"))
		Params = TEXT("self, button, ...");
	else if (ScriptName == TEXT("OnReceiveDrag"))
		Params = TEXT("self, ...");
	else
		Params = TEXT("self, ...");

	FString Wrapped = FString::Printf(TEXT("return function(%s) %s end"), *Params, *Code);
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

void FWowEventSystem::SetFrameScriptRef(int64 Handle, const FString& ScriptName, int32 LuaRef)
{
#if HAS_LUA
	if (!LuaVM) return;
	lua_State* L = LuaVM->GetState();
	if (!L) return;

	// Remove old ref if any
	TMap<FString, int32>& FrameScripts = ScriptRefs.FindOrAdd(Handle);
	if (int32* OldRef = FrameScripts.Find(ScriptName))
	{
		luaL_unref(L, LUA_REGISTRYINDEX, *OldRef);
	}
	FrameScripts.Add(ScriptName, LuaRef);

	// Track OnUpdate frames
	if (ScriptName == TEXT("OnUpdate"))
	{
		OnUpdateFrames.Add(Handle);
	}

	UE_LOG(LogWowEvent, Verbose, TEXT("Stored script ref [%lld] %s (ref %d)"), Handle, *ScriptName, LuaRef);
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

void FWowEventSystem::CreateTextureRegionGlobal(const FString& RegionName)
{
#if HAS_LUA
	if (!LuaVM || RegionName.IsEmpty()) return;

	lua_State* L = LuaVM->GetState();
	if (!L) return;

	// Create a Lua table for this texture region with the frame metatable
	lua_newtable(L);

	// Store the name
	FTCHARToUTF8 UTF8Name(*RegionName);
	lua_pushstring(L, UTF8Name.Get());
	lua_setfield(L, -2, "__name");

	// Set the frame metatable so region:SetTexture() etc work
	luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
	if (!lua_isnil(L, -1))
	{
		lua_setmetatable(L, -2);
	}
	else
	{
		lua_pop(L, 1);
	}

	// Register as global
	lua_setglobal(L, UTF8Name.Get());
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

// Helper: given "PlayerFrameHealthBar" and parent "PlayerFrame", return "healthBar"
static FString GetChildFieldKey(const FString& ChildName, const FString& ParentName)
{
	if (ParentName.IsEmpty() || ChildName.Len() <= ParentName.Len()) return FString();
	if (!ChildName.StartsWith(ParentName)) return FString();
	FString Suffix = ChildName.Mid(ParentName.Len());
	if (Suffix.IsEmpty()) return FString();
	// Lowercase first character of suffix
	Suffix[0] = FChar::ToLower(Suffix[0]);
	return Suffix;
}

void FWowEventSystem::CompileFrameScripts(int64 Handle, const FWowFrameDef& Def)
{
	// Create the Lua frame object first
	CreateFrameObject(Handle, Def.Name);

#if HAS_LUA
	// Set child element references on the parent frame table.
	// WoW expects frame.healthBar, frame.text, frame.icon etc. to be set
	// based on named children (ChildName minus ParentName prefix, first letter lowercased).
	if (LuaVM && !Def.Name.IsEmpty())
	{
		lua_State* L = LuaVM->GetState();
		int32* FrameRef = FrameObjectRefs.Find(Handle);
		if (L && FrameRef)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, *FrameRef); // push frame table

			// Set the frame ID from XML "id" attribute
			if (Def.FrameID != 0)
			{
				lua_pushnumber(L, Def.FrameID);
				lua_setfield(L, -2, "__id");
			}

			// Map named textures and fontstrings from layers
			int32 MappedCount = 0, MissedCount = 0;
			bool bDebugThisFrame = Def.Name.Contains(TEXT("OptionsFrameTemplateCategoryFrameButton1")) ||
			                        Def.Name.Contains(TEXT("TargetFrame")) ||
			                        Def.Name.Contains(TEXT("PartyMemberFrame1"));
			if (bDebugThisFrame)
			{
				UE_LOG(LogWowEvent, Warning, TEXT("CHILDMAP: %s has %d layers, %d children"), *Def.Name, Def.Layers.Num(), Def.Children.Num());
				for (const FWowLayer& DL : Def.Layers) {
					for (const FWowTextureElement& DT : DL.Textures) UE_LOG(LogWowEvent, Warning, TEXT("  TEX: '%s'"), *DT.Name);
					for (const FWowFontStringElement& DFS : DL.FontStrings) UE_LOG(LogWowEvent, Warning, TEXT("  FS: '%s'"), *DFS.Name);
				}
				for (const FWowFrameDef& DC : Def.Children) UE_LOG(LogWowEvent, Warning, TEXT("  CHILD: '%s'"), *DC.Name);
			}
			for (const FWowLayer& Layer : Def.Layers)
			{
				for (const FWowTextureElement& Tex : Layer.Textures)
				{
					if (Tex.Name.IsEmpty()) continue;
					FString Key = GetChildFieldKey(Tex.Name, Def.Name);
					if (Key.IsEmpty()) continue;
					lua_getglobal(L, TCHAR_TO_UTF8(*Tex.Name));
					if (!lua_isnil(L, -1))
					{
						lua_setfield(L, -2, TCHAR_TO_UTF8(*Key));
					}
					else
					{
						lua_pop(L, 1);
					}
				}
				for (const FWowFontStringElement& FS : Layer.FontStrings)
				{
					if (FS.Name.IsEmpty()) continue;
					FString Key = GetChildFieldKey(FS.Name, Def.Name);
					if (Key.IsEmpty()) continue;
					lua_getglobal(L, TCHAR_TO_UTF8(*FS.Name));
					if (!lua_isnil(L, -1))
					{
						lua_setfield(L, -2, TCHAR_TO_UTF8(*Key));
					}
					else
					{
						lua_pop(L, 1);
					}
				}
			}

			// Map named child frames
			for (const FWowFrameDef& Child : Def.Children)
			{
				if (Child.Name.IsEmpty()) continue;
				FString Key = GetChildFieldKey(Child.Name, Def.Name);
				if (Key.IsEmpty()) continue;
				lua_getglobal(L, TCHAR_TO_UTF8(*Child.Name));
				if (!lua_isnil(L, -1))
				{
					lua_setfield(L, -2, TCHAR_TO_UTF8(*Key));
				}
				else
				{
					lua_pop(L, 1);
				}
			}

			// Map button texture names
			auto SetButtonTexRef = [&](const FString& TexName) {
				if (TexName.IsEmpty()) return;
				FString Key = GetChildFieldKey(TexName, Def.Name);
				if (Key.IsEmpty()) return;
				lua_getglobal(L, TCHAR_TO_UTF8(*TexName));
				if (!lua_isnil(L, -1))
					lua_setfield(L, -2, TCHAR_TO_UTF8(*Key));
				else
					lua_pop(L, 1);
			};
			SetButtonTexRef(Def.NormalTextureName);
			SetButtonTexRef(Def.PushedTextureName);
			SetButtonTexRef(Def.HighlightTextureName);
			SetButtonTexRef(Def.DisabledTextureName);

			// NOTE: Do NOT set self.name = self:GetName() here.
			// In WoW, self.name refers to the child FontString (FrameNameName),
			// NOT the frame name string. self:GetName() returns the name via __name.

			// For buttons: auto-create child references for common WoW patterns.
			// WoW buttons have ButtonText -> creates FrameNameText fontstring
			// Also NormalTexture/PushedTexture/etc. -> creates FrameNameNormalTexture etc.
			if (!Def.Name.IsEmpty())
			{
				// Auto-create "text" field pointing to _G[frameName.."Text"] (button text fontstring)
				FString TextGlobalName = Def.Name + TEXT("Text");
				lua_getglobal(L, TCHAR_TO_UTF8(*TextGlobalName));
				if (lua_isnil(L, -1))
				{
					lua_pop(L, 1);
					// Create a stub FontString table for FrameNameText and set as global + field
					CreateTextureRegionGlobal(TextGlobalName);
					lua_getglobal(L, TCHAR_TO_UTF8(*TextGlobalName));
				}
				if (!lua_isnil(L, -1))
				{
					lua_setfield(L, -2, "text");
				}
				else
				{
					lua_pop(L, 1);
				}

				// Auto-create common child fields: icon, background, checkButton, normalTexture, etc.
				auto TrySetChildField = [&](const char* suffix) {
					FString GlobalName = Def.Name + UTF8_TO_TCHAR(suffix);
					lua_getglobal(L, TCHAR_TO_UTF8(*GlobalName));
					if (!lua_isnil(L, -1))
					{
						// Lowercase first char of suffix for the field key
						FString Key = UTF8_TO_TCHAR(suffix);
						Key[0] = FChar::ToLower(Key[0]);
						lua_setfield(L, -2, TCHAR_TO_UTF8(*Key));
					}
					else
					{
						lua_pop(L, 1);
					}
				};
				TrySetChildField("Icon");
				TrySetChildField("Background");
				TrySetChildField("CheckButton");
				TrySetChildField("Check");
				TrySetChildField("NormalText");
				TrySetChildField("HighlightText");
				TrySetChildField("DisabledText");
				TrySetChildField("Count");
				TrySetChildField("Name");
				TrySetChildField("HealthBar");
				TrySetChildField("ManaBar");
				TrySetChildField("Portrait");
				TrySetChildField("Texture");
				TrySetChildField("Border");
				TrySetChildField("Flash");
				TrySetChildField("Cooldown");
				TrySetChildField("HotKey");
				TrySetChildField("StatusBar");
				TrySetChildField("ScrollBar");
				TrySetChildField("ScrollBarBackground");
				TrySetChildField("OverflowButton");
				TrySetChildField("EnableButton");
				TrySetChildField("ButtonFrame");

				// For critical children that FrameXML expects but may not exist as
				// globals (e.g., dock manager overflow button), create stub tables
				// so OnLoad handlers don't crash.
				auto EnsureChildField = [&](const char* fieldName) {
					lua_getfield(L, -1, fieldName);
					if (lua_isnil(L, -1))
					{
						lua_pop(L, 1);
						// Create a stub table with metatable
						lua_newtable(L);
						luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
						if (!lua_isnil(L, -1))
							lua_setmetatable(L, -2);
						else
							lua_pop(L, 1);
						lua_setfield(L, -2, fieldName);
					}
					else
					{
						lua_pop(L, 1);
					}
				};
				EnsureChildField("overflowButton");
				EnsureChildField("buttonFrame");
				EnsureChildField("scrollBar");
				EnsureChildField("scrollBarBackground");
				EnsureChildField("scrollBarArtTop");
				EnsureChildField("scrollBarArtBottom");
				// UnitFrame-critical: name, portrait, healthbar, manabar, etc.
				EnsureChildField("name");
				// Create alias globals for FrameName+Suffix patterns.
				// WoW FrameXML code like UnitFrame_OnLoad does:
				//   self.name = _G[self:GetName().."Name"]
				// But the actual FontString might be nested deeper (e.g.,
				// Boss1TargetFrameTextureFrameName instead of Boss1TargetFrameName).
				// Create an alias or stub if the direct global doesn't exist.
				auto EnsureGlobalOrAlias = [&](const char* suffix) {
					FString DirectName = Def.Name + UTF8_TO_TCHAR(suffix);
					lua_getglobal(L, TCHAR_TO_UTF8(*DirectName));
					if (!lua_isnil(L, -1))
					{
						lua_pop(L, 1);
						return; // Direct global exists
					}
					lua_pop(L, 1);

					// Search for a nested child global that ends with the suffix
					// e.g., Boss1TargetFrameTextureFrameName for suffix "Name"
					bool bFound = false;
					for (const FWowFrameDef& Child : Def.Children)
					{
						if (Child.Name.IsEmpty()) continue;
						FString ChildGlobal = Child.Name + UTF8_TO_TCHAR(suffix);
						lua_getglobal(L, TCHAR_TO_UTF8(*ChildGlobal));
						if (!lua_isnil(L, -1))
						{
							// Found nested child global — create alias
							lua_setglobal(L, TCHAR_TO_UTF8(*DirectName));
							bFound = true;
							break;
						}
						lua_pop(L, 1);
					}

					if (!bFound)
					{
						CreateTextureRegionGlobal(DirectName);
					}
				};
				EnsureGlobalOrAlias("Name");
				EnsureGlobalOrAlias("HealthBar");
				EnsureGlobalOrAlias("ManaBar");
				EnsureGlobalOrAlias("Portrait");
				EnsureGlobalOrAlias("NumericalThreat");
				EnsureGlobalOrAlias("NormalText");
				EnsureGlobalOrAlias("HighlightText");
				EnsureGlobalOrAlias("DisabledText");
				EnsureGlobalOrAlias("Text");
				EnsureGlobalOrAlias("DropDown");
				EnsureChildField("portrait");
				EnsureChildField("deadText");
				EnsureChildField("unconsciousText");
				EnsureChildField("threatIndicator");
				EnsureChildField("leaderIcon");
				EnsureChildField("raidTargetIcon");
				EnsureChildField("readyCheckIcon");
				// ChatFrame/DockManager
				EnsureChildField("list");
				EnsureChildField("editBox");
				// LFD/LFR role buttons
				EnsureChildField("checkButton");
				// Dropdown menu
				EnsureChildField("normalText");
				EnsureChildField("highlightText");
				EnsureChildField("disabledText");
				// Unit popup / dropdown
				EnsureChildField("dropdownMenu");
				EnsureChildField("dropDown");
				// ScrollFrame child
				EnsureChildField("scrollFrame");
				// Class color legend
				EnsureChildField("firstClass");
				// Chat config class color legend
				EnsureChildField("header");
				// ChatFrame selected textures (FloatingChatFrame.lua)
				EnsureChildField("leftSelectedTexture");
				EnsureChildField("middleSelectedTexture");
				EnsureChildField("rightSelectedTexture");
				EnsureChildField("leftHighlightTexture");
				EnsureChildField("middleHighlightTexture");
				EnsureChildField("rightHighlightTexture");
			}

			lua_pop(L, 1); // pop frame table
		}
	}
#endif

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

	// Fire OnLoad immediately after compilation (WoW fires OnLoad when frame is created)
	TMap<FString, int32>* FrameScripts = ScriptRefs.Find(Handle);
	if (FrameScripts)
	{
		int32* OnLoadRef = FrameScripts->Find(TEXT("OnLoad"));
		if (OnLoadRef)
		{
			lua_State* L = LuaVM ? LuaVM->GetState() : nullptr;
			if (L)
			{
				lua_rawgeti(L, LUA_REGISTRYINDEX, *OnLoadRef);
				if (lua_isfunction(L, -1))
				{
					// Push self (frame table)
					int32* FrameRef = FrameObjectRefs.Find(Handle);
					if (FrameRef)
						lua_rawgeti(L, LUA_REGISTRYINDEX, *FrameRef);
					else
						lua_newtable(L);

					if (lua_pcall(L, 1, 0, 0) != 0)
					{
						UE_LOG(LogWowEvent, Error, TEXT("OnLoad error [%lld] %s: %hs"),
							Handle, *Def.Name, lua_tostring(L, -1));
						lua_pop(L, 1);

						// Hide frames whose OnLoad crashed — incomplete init may show broken UI
						if (FrameManager)
						{
							FrameManager->SetFrameVisible(Handle, false);
						}
					}
				}
				else
				{
					lua_pop(L, 1);
				}
			}
		}
	}
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
			UE_LOG(LogWowEvent, Warning, TEXT("OnUpdate error [frame %lld]: %s — disabling OnUpdate for this frame"),
				Handle, UTF8_TO_TCHAR(lua_tostring(L, -1)));
			lua_pop(L, 1);
			// Disable further OnUpdate calls for this erroring frame to prevent spam
			OnUpdateFrames.Remove(Handle);
			break; // Iterator invalidated
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
	case 0x006E: return TEXT("PLAYER_LEAVING_WORLD");            // SMSG_LOGOUT_COMPLETE

	// Targeting
	case 0x0078: return TEXT("PLAYER_TARGET_CHANGED");           // MSG_RAID_TARGET_UPDATE (contextual)

	// Unit updates
	case 0x0480: return TEXT("UNIT_MANA");                       // SMSG_POWER_UPDATE (correct opcode)
	case 0x0495: return TEXT("UNIT_AURA");                       // SMSG_AURA_UPDATE (correct opcode)
	case 0x0051: return TEXT("UNIT_NAME_UPDATE");                // SMSG_NAME_QUERY_RESPONSE (correct opcode)

	// Chat messages
	case 0x0096: return TEXT("CHAT_MSG_SAY");                    // SMSG_MESSAGECHAT (filtered by type)
	case 0x0097: return TEXT("CHAT_MSG_WHISPER");                // SMSG_MESSAGECHAT
	case 0x0098: return TEXT("CHAT_MSG_PARTY");                  // SMSG_MESSAGECHAT
	case 0x0099: return TEXT("CHAT_MSG_GUILD");                  // SMSG_MESSAGECHAT
	case 0x009A: return TEXT("CHAT_MSG_YELL");                   // SMSG_MESSAGECHAT

	// Spells and actions
	case 0x012A: return TEXT("SPELLS_CHANGED");                  // SMSG_INITIAL_SPELLS (correct opcode)
	case 0x012B: return TEXT("LEARNED_SPELL_IN_TAB");            // SMSG_LEARNED_SPELL
	case 0x0130: return TEXT("SPELL_UPDATE_COOLDOWN");           // SMSG_SPELL_COOLDOWN (correct opcode)
	case 0x0129: return TEXT("ACTIONBAR_SLOT_CHANGED");          // SMSG_ACTION_BUTTONS (correct opcode)

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
	case 0x008A: return TEXT("GUILD_ROSTER_UPDATE");             // SMSG_GUILD_ROSTER (correct opcode)
	case 0x0092: return TEXT("GUILD_MOTD");                      // SMSG_GUILD_EVENT

	// Loot
	case 0x0160: return TEXT("LOOT_OPENED");                     // SMSG_LOOT_RESPONSE
	case 0x0161: return TEXT("LOOT_CLOSED");                     // SMSG_LOOT_RELEASE_RESPONSE

	// Mail
	case 0x023B: return TEXT("MAIL_INBOX_UPDATE");               // SMSG_MAIL_LIST_RESULT (correct opcode)

	// Talent / skills
	case 0x012F: return TEXT("CHARACTER_POINTS_CHANGED");
	case 0x04C0: return TEXT("PLAYER_TALENT_UPDATE");            // SMSG_TALENTS_INFO (correct opcode)
	case 0x0127: return TEXT("SKILL_LINES_CHANGED");             // SMSG_SET_PROFICIENCY

	// Merchant / vendor
	case 0x019F: return TEXT("MERCHANT_SHOW");                   // SMSG_LIST_INVENTORY
	case 0x01B4: return TEXT("MERCHANT_CLOSED");

	// Friend list
	case 0x0067: return TEXT("FRIENDLIST_UPDATE");               // SMSG_FRIEND_LIST (correct opcode)

	// Battleground / PvP
	case 0x02E8: return TEXT("UPDATE_BATTLEFIELD_STATUS");       // SMSG_BATTLEFIELD_STATUS
	case 0x02EC: return TEXT("BATTLEFIELDS_SHOW");

	// Achievement
	case 0x0468: return TEXT("ACHIEVEMENT_EARNED");              // SMSG_ACHIEVEMENT_EARNED

	// Player progression
	case 0x01D0: return TEXT("COMBAT_XP_GAIN");                  // SMSG_LOG_XPGAIN
	case 0x01D4: return TEXT("PLAYER_LEVEL_UP");                 // SMSG_LEVELUP_INFO
	case 0x01F8: return TEXT("ZONE_UNDER_ATTACK");               // SMSG_EXPLORATION_EXPERIENCE

	// Weather/Environment
	case 0x02F4: return TEXT("WEATHER_CHANGED");                 // SMSG_WEATHER
	// Note: 0x02C3 already mapped to ZONE_CHANGED above — UPDATE_WORLD_STATE fires programmatically

	// Death / resurrection
	case 0x02E7: return TEXT("PLAYER_DEAD");
	case 0x0163: return TEXT("PLAYER_ALIVE");
	case 0x015E: return TEXT("CORPSE_IN_RANGE");

	default:
		return FString();
	}
}
