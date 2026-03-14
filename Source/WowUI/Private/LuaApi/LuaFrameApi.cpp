#include "LuaApiRegistry.h"
#include "WowEventSystem.h"
#include "WowFrameManager.h"
#include "WowFrameTypes.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

DEFINE_LOG_CATEGORY_STATIC(LogWowLuaFrame, Log, All);

// ── Helpers ──────────────────────────────────────────────────────────────────────

// Get the frame handle from a frame table (self argument at ArgIdx)
static int64 GetFrameHandle(lua_State* L, int ArgIdx = 1)
{
	luaL_checktype(L, ArgIdx, LUA_TTABLE);
	lua_getfield(L, ArgIdx, "__handle");
	int64 Handle = static_cast<int64>(lua_tointeger(L, -1));
	lua_pop(L, 1);
	return Handle;
}

// ── Global: CreateFrame(type, name, parent, template) ────────────────────────────

static EWowFrameType ParseFrameType(const char* TypeStr)
{
	if (!TypeStr) return EWowFrameType::Frame;
	if (strcmp(TypeStr, "Button") == 0) return EWowFrameType::Button;
	if (strcmp(TypeStr, "CheckButton") == 0) return EWowFrameType::CheckButton;
	if (strcmp(TypeStr, "EditBox") == 0) return EWowFrameType::EditBox;
	if (strcmp(TypeStr, "ScrollFrame") == 0) return EWowFrameType::ScrollFrame;
	if (strcmp(TypeStr, "Slider") == 0) return EWowFrameType::Slider;
	if (strcmp(TypeStr, "StatusBar") == 0) return EWowFrameType::StatusBar;
	if (strcmp(TypeStr, "SimpleHTML") == 0) return EWowFrameType::SimpleHTML;
	if (strcmp(TypeStr, "Cooldown") == 0) return EWowFrameType::Cooldown;
	if (strcmp(TypeStr, "GameTooltip") == 0) return EWowFrameType::GameTooltip;
	if (strcmp(TypeStr, "Minimap") == 0) return EWowFrameType::Minimap;
	if (strcmp(TypeStr, "Model") == 0) return EWowFrameType::Model;
	if (strcmp(TypeStr, "PlayerModel") == 0) return EWowFrameType::PlayerModel;
	if (strcmp(TypeStr, "MessageFrame") == 0) return EWowFrameType::MessageFrame;
	if (strcmp(TypeStr, "ScrollingMessageFrame") == 0) return EWowFrameType::ScrollingMessageFrame;
	return EWowFrameType::Frame;
}

static int L_CreateFrame(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (!Ctx || !Ctx->FrameManager || !Ctx->EventSystem) { lua_pushnil(L); return 1; }

	const char* TypeStr = luaL_optstring(L, 1, "Frame");
	const char* Name = luaL_optstring(L, 2, nullptr);
	// arg 3 = parent (frame table or nil)
	const char* TemplateName = luaL_optstring(L, 4, nullptr);

	FWowFrameDef Def;
	Def.Type = ParseFrameType(TypeStr);
	if (Name) Def.Name = UTF8_TO_TCHAR(Name);
	if (TemplateName) Def.Inherits = UTF8_TO_TCHAR(TemplateName);

	// Resolve parent
	if (lua_istable(L, 3))
	{
		lua_getfield(L, 3, "__name");
		if (lua_isstring(L, -1))
		{
			Def.Parent = UTF8_TO_TCHAR(lua_tostring(L, -1));
		}
		lua_pop(L, 1);
	}
	else if (lua_isstring(L, 3))
	{
		Def.Parent = UTF8_TO_TCHAR(lua_tostring(L, 3));
	}

	int64 Handle = Ctx->FrameManager->CreateFrame(Def);
	if (Handle < 0) { lua_pushnil(L); return 1; }

	// The frame object was created by CreateFrame -> CompileFrameScripts -> CreateFrameObject
	// Retrieve it from the Lua global if named, or create a minimal one
	if (Name)
	{
		lua_getglobal(L, Name);
		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			lua_pushnil(L);
		}
	}
	else
	{
		// For unnamed frames, create a frame object now
		Ctx->EventSystem->CreateFrameObject(Handle, FString());
		// Retrieve from __handle lookup won't work without a ref, so we push a table
		lua_newtable(L);
		lua_pushinteger(L, Handle);
		lua_setfield(L, -2, "__handle");
		luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
		lua_setmetatable(L, -2);
	}
	return 1;
}

// ── Frame Methods ────────────────────────────────────────────────────────────────

// frame:GetName()
static int LF_GetName(lua_State* L)
{
	lua_getfield(L, 1, "__name");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushnil(L);
	}
	return 1;
}

// frame:GetObjectType()
static int LF_GetObjectType(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		if (Def)
		{
			switch (Def->Type)
			{
			case EWowFrameType::Button: lua_pushstring(L, "Button"); break;
			case EWowFrameType::EditBox: lua_pushstring(L, "EditBox"); break;
			case EWowFrameType::Slider: lua_pushstring(L, "Slider"); break;
			case EWowFrameType::StatusBar: lua_pushstring(L, "StatusBar"); break;
			default: lua_pushstring(L, "Frame"); break;
			}
			return 1;
		}
	}
	lua_pushstring(L, "Frame");
	return 1;
}

// frame:RegisterEvent(event)
static int LF_RegisterEvent(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	const char* Event = luaL_checkstring(L, 2);
	if (Ctx && Ctx->EventSystem)
	{
		Ctx->EventSystem->RegisterEvent(Handle, UTF8_TO_TCHAR(Event));
	}
	return 0;
}

// frame:UnregisterEvent(event)
static int LF_UnregisterEvent(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	const char* Event = luaL_checkstring(L, 2);
	if (Ctx && Ctx->EventSystem)
	{
		Ctx->EventSystem->UnregisterEvent(Handle, UTF8_TO_TCHAR(Event));
	}
	return 0;
}

// frame:UnregisterAllEvents()
static int LF_UnregisterAllEvents(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->EventSystem)
	{
		Ctx->EventSystem->UnregisterAllEvents(Handle);
	}
	return 0;
}

// frame:SetScript(scriptType, handler)
static int LF_SetScript(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	const char* ScriptType = luaL_checkstring(L, 2);

	if (Ctx && Ctx->EventSystem)
	{
		if (lua_isfunction(L, 3))
		{
			// Store the function as a script reference directly
			// We need to wrap this: store function ref and call it in FireEvent
			// Use the event system's script storage via a Lua indirection
			FString Key = FString::Printf(TEXT("__script_%lld_%s"), Handle, UTF8_TO_TCHAR(ScriptType));
			FTCHARToUTF8 UTF8Key(*Key);

			lua_pushvalue(L, 3); // push the function
			lua_setfield(L, LUA_REGISTRYINDEX, UTF8Key.Get());

			// Create a wrapper code that calls the stored function
			FString WrapperCode = FString::Printf(
				TEXT("local f = rawget(_G, '__NOOP'); return function(self, event, ...) ")
				TEXT("local fn = _R['%s']; if fn then fn(self, event, ...) end end"),
				*Key);

			// Simpler approach: directly store the function via the event system
			// Store function ref in registry
			lua_pushvalue(L, 3);
			int Ref = luaL_ref(L, LUA_REGISTRYINDEX);

			// Create a tiny wrapper that calls the ref'd function
			FString Code = FString::Printf(
				TEXT("local __ref=%d; return function(self,event,...) ")
				TEXT("local f=__WowGetRef(%d); if f then f(self,event,...) end end"),
				Ref, Ref);

			// Actually, the simplest approach: store the Lua function ref directly
			// in the event system's script ref map
			// We can access the ScriptRefs through SetFrameScript but it expects code...
			// Let's just store the function ref directly
			// Push function, store as registry ref, set in event system's map

			// Undo the ref we just made - we'll store it differently
			luaL_unref(L, LUA_REGISTRYINDEX, Ref);

			// Directly store: push function, luaL_ref, store in event system
			lua_pushvalue(L, 3);
			// We need the event system to accept a raw Lua ref...
			// For now, let's use a simple global lookup approach

			// Store function in a known global key
			lua_pushvalue(L, 3);
			lua_setfield(L, LUA_REGISTRYINDEX, UTF8Key.Get());

			// Compile wrapper code that retrieves and calls it
			FString WrapCode = FString::Printf(
				TEXT("return function(self, event, ...) ")
				TEXT("local _R = debug and debug.getregistry and debug.getregistry() ")
				TEXT("end"));

			// Simplest working approach: store function in a global, compile a stub
			// that calls it
			FString GlobalKey = FString::Printf(TEXT("__WowScript_%lld_%s"),
				Handle, UTF8_TO_TCHAR(ScriptType));
			FTCHARToUTF8 UTF8GlobalKey(*GlobalKey);

			lua_pushvalue(L, 3);
			lua_setglobal(L, UTF8GlobalKey.Get());

			FString StubCode = FString::Printf(TEXT("%s(self, event, ...)"),
				*GlobalKey);
			Ctx->EventSystem->SetFrameScript(Handle, UTF8_TO_TCHAR(ScriptType), StubCode);
		}
		else if (lua_isnil(L, 3))
		{
			// Clear the script - set empty code
			Ctx->EventSystem->SetFrameScript(Handle, UTF8_TO_TCHAR(ScriptType), FString());
		}
	}
	return 0;
}

// frame:GetScript(scriptType)
static int LF_GetScript(lua_State* L)
{
	int64 Handle = GetFrameHandle(L);
	const char* ScriptType = luaL_checkstring(L, 2);

	// Try to find the stored function in the global
	FString GlobalKey = FString::Printf(TEXT("__WowScript_%lld_%s"),
		Handle, UTF8_TO_TCHAR(ScriptType));
	FTCHARToUTF8 UTF8Key(*GlobalKey);

	lua_getglobal(L, UTF8Key.Get());
	if (lua_isfunction(L, -1))
	{
		return 1;
	}
	lua_pop(L, 1);
	lua_pushnil(L);
	return 1;
}

// frame:Show()
static int LF_Show(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		Ctx->FrameManager->SetFrameVisible(Handle, true);
	}
	return 0;
}

// frame:Hide()
static int LF_Hide(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		Ctx->FrameManager->SetFrameVisible(Handle, false);
	}
	return 0;
}

// frame:IsShown()
static int LF_IsShown(lua_State* L)
{
	// TODO: track visibility state properly
	lua_pushboolean(L, 1);
	return 1;
}

// frame:IsVisible()
static int LF_IsVisible(lua_State* L)
{
	lua_pushboolean(L, 1);
	return 1;
}

// frame:SetPoint(point, relativeTo, relativePoint, ofsx, ofsy)
static int LF_SetPoint(lua_State* L)
{
	// Store anchor data on the frame table for later use
	// In a full implementation this would update the UMG widget position
	return 0;
}

// frame:ClearAllPoints()
static int LF_ClearAllPoints(lua_State* L)
{
	return 0;
}

// frame:SetAllPoints(relativeTo)
static int LF_SetAllPoints(lua_State* L)
{
	return 0;
}

// frame:SetSize(width, height)
static int LF_SetSize(lua_State* L)
{
	return 0;
}

// frame:SetWidth(width)
static int LF_SetWidth(lua_State* L)
{
	return 0;
}

// frame:SetHeight(height)
static int LF_SetHeight(lua_State* L)
{
	return 0;
}

// frame:GetWidth()
static int LF_GetWidth(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		if (Def)
		{
			lua_pushnumber(L, Def->Width);
			return 1;
		}
	}
	lua_pushnumber(L, 0);
	return 1;
}

// frame:GetHeight()
static int LF_GetHeight(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		if (Def)
		{
			lua_pushnumber(L, Def->Height);
			return 1;
		}
	}
	lua_pushnumber(L, 0);
	return 1;
}

// frame:SetAlpha(alpha)
static int LF_SetAlpha(lua_State* L)
{
	return 0;
}

// frame:GetAlpha()
static int LF_GetAlpha(lua_State* L)
{
	lua_pushnumber(L, 1.0);
	return 1;
}

// frame:SetFrameStrata(strata)
static int LF_SetFrameStrata(lua_State* L)
{
	return 0;
}

// frame:GetFrameStrata()
static int LF_GetFrameStrata(lua_State* L)
{
	lua_pushstring(L, "MEDIUM");
	return 1;
}

// frame:SetFrameLevel(level)
static int LF_SetFrameLevel(lua_State* L)
{
	return 0;
}

// frame:GetFrameLevel()
static int LF_GetFrameLevel(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		if (Def)
		{
			lua_pushnumber(L, Def->FrameLevel);
			return 1;
		}
	}
	lua_pushnumber(L, 0);
	return 1;
}

// frame:EnableMouse(enable)
static int LF_EnableMouse(lua_State* L)
{
	return 0;
}

// frame:EnableKeyboard(enable)
static int LF_EnableKeyboard(lua_State* L)
{
	return 0;
}

// frame:EnableMouseWheel(enable)
static int LF_EnableMouseWheel(lua_State* L)
{
	return 0;
}

// frame:SetMovable(movable)
static int LF_SetMovable(lua_State* L)
{
	return 0;
}

// frame:SetResizable(resizable)
static int LF_SetResizable(lua_State* L)
{
	return 0;
}

// frame:SetClampedToScreen(clamped)
static int LF_SetClampedToScreen(lua_State* L)
{
	return 0;
}

// frame:SetParent(parent)
static int LF_SetParent(lua_State* L)
{
	return 0;
}

// frame:GetParent()
static int LF_GetParent(lua_State* L)
{
	lua_pushnil(L);
	return 1;
}

// frame:GetChildren()
static int LF_GetChildren(lua_State* L)
{
	return 0;
}

// frame:GetNumChildren()
static int LF_GetNumChildren(lua_State* L)
{
	lua_pushnumber(L, 0);
	return 1;
}

// frame:CreateTexture(name, layer, inherits)
static int LF_CreateTexture(lua_State* L)
{
	// Create a minimal texture table with a metatable
	lua_newtable(L);

	// Stub texture methods
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetTexture
	lua_setfield(L, -2, "SetTexture");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetTexCoord
	lua_setfield(L, -2, "SetTexCoord");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetVertexColor
	lua_setfield(L, -2, "SetVertexColor");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetAlpha
	lua_setfield(L, -2, "SetAlpha");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // Show
	lua_setfield(L, -2, "Show");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // Hide
	lua_setfield(L, -2, "Hide");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetPoint
	lua_setfield(L, -2, "SetPoint");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // ClearAllPoints
	lua_setfield(L, -2, "ClearAllPoints");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetAllPoints
	lua_setfield(L, -2, "SetAllPoints");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetSize
	lua_setfield(L, -2, "SetSize");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetWidth
	lua_setfield(L, -2, "SetWidth");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetHeight
	lua_setfield(L, -2, "SetHeight");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; }); // GetWidth
	lua_setfield(L, -2, "GetWidth");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; }); // GetHeight
	lua_setfield(L, -2, "GetHeight");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushboolean(L2, 1); return 1; }); // IsShown
	lua_setfield(L, -2, "IsShown");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushstring(L2, "Texture"); return 1; }); // GetObjectType
	lua_setfield(L, -2, "GetObjectType");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetBlendMode
	lua_setfield(L, -2, "SetBlendMode");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetDesaturated
	lua_setfield(L, -2, "SetDesaturated");

	return 1;
}

// frame:CreateFontString(name, layer, inherits)
static int LF_CreateFontString(lua_State* L)
{
	lua_newtable(L);

	// Stub FontString methods
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetText
	lua_setfield(L, -2, "SetText");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushstring(L2, ""); return 1; }); // GetText
	lua_setfield(L, -2, "GetText");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetTextColor
	lua_setfield(L, -2, "SetTextColor");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetFont
	lua_setfield(L, -2, "SetFont");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetFontObject
	lua_setfield(L, -2, "SetFontObject");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushstring(L2, ""); lua_pushnumber(L2, 12); lua_pushstring(L2, ""); return 3; }); // GetFont
	lua_setfield(L, -2, "GetFont");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetJustifyH
	lua_setfield(L, -2, "SetJustifyH");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetJustifyV
	lua_setfield(L, -2, "SetJustifyV");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetPoint
	lua_setfield(L, -2, "SetPoint");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // ClearAllPoints
	lua_setfield(L, -2, "ClearAllPoints");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetAllPoints
	lua_setfield(L, -2, "SetAllPoints");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // Show
	lua_setfield(L, -2, "Show");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // Hide
	lua_setfield(L, -2, "Hide");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetAlpha
	lua_setfield(L, -2, "SetAlpha");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetShadowOffset
	lua_setfield(L, -2, "SetShadowOffset");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetShadowColor
	lua_setfield(L, -2, "SetShadowColor");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; }); // GetStringWidth
	lua_setfield(L, -2, "GetStringWidth");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; }); // GetStringHeight
	lua_setfield(L, -2, "GetStringHeight");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushstring(L2, "FontString"); return 1; }); // GetObjectType
	lua_setfield(L, -2, "GetObjectType");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetFormattedText
	lua_setfield(L, -2, "SetFormattedText");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetWordWrap
	lua_setfield(L, -2, "SetWordWrap");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetNonSpaceWrap
	lua_setfield(L, -2, "SetNonSpaceWrap");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetMaxLines
	lua_setfield(L, -2, "SetMaxLines");

	return 1;
}

// frame:SetBackdrop(backdrop)
static int LF_SetBackdrop(lua_State* L)
{
	return 0;
}

// frame:SetBackdropColor(r, g, b, a)
static int LF_SetBackdropColor(lua_State* L)
{
	return 0;
}

// frame:SetBackdropBorderColor(r, g, b, a)
static int LF_SetBackdropBorderColor(lua_State* L)
{
	return 0;
}

// frame:SetID(id)
static int LF_SetID(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushvalue(L, 2);
	lua_setfield(L, 1, "__id");
	return 0;
}

// frame:GetID()
static int LF_GetID(lua_State* L)
{
	lua_getfield(L, 1, "__id");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushnumber(L, 0);
	}
	return 1;
}

// frame:SetAttribute(name, value)
static int LF_SetAttribute(lua_State* L)
{
	return 0;
}

// frame:GetAttribute(name)
static int LF_GetAttribute(lua_State* L)
{
	lua_pushnil(L);
	return 1;
}

// frame:HookScript(scriptType, handler)
static int LF_HookScript(lua_State* L)
{
	// For now, just behave like SetScript
	return LF_SetScript(L);
}

// frame:HasScript(scriptType)
static int LF_HasScript(lua_State* L)
{
	lua_pushboolean(L, 1);
	return 1;
}

// frame:GetCenter()
static int LF_GetCenter(lua_State* L)
{
	lua_pushnumber(L, 0);
	lua_pushnumber(L, 0);
	return 2;
}

// frame:GetLeft/Right/Top/Bottom
static int LF_GetLeft(lua_State* L) { lua_pushnumber(L, 0); return 1; }
static int LF_GetRight(lua_State* L) { lua_pushnumber(L, 0); return 1; }
static int LF_GetTop(lua_State* L) { lua_pushnumber(L, 0); return 1; }
static int LF_GetBottom(lua_State* L) { lua_pushnumber(L, 0); return 1; }

// frame:SetTexture(texture) — for Button Normal/Pushed/etc textures
static int LF_SetNormalTexture(lua_State* L) { return 0; }
static int LF_SetPushedTexture(lua_State* L) { return 0; }
static int LF_SetHighlightTexture(lua_State* L) { return 0; }
static int LF_SetDisabledTexture(lua_State* L) { return 0; }

// Button methods
static int LF_SetText(lua_State* L) { return 0; }
static int LF_GetText(lua_State* L) { lua_pushstring(L, ""); return 1; }
static int LF_Disable(lua_State* L) { return 0; }
static int LF_Enable(lua_State* L) { return 0; }
static int LF_IsEnabled(lua_State* L) { lua_pushboolean(L, 1); return 1; }
static int LF_Click(lua_State* L) { return 0; }

// StatusBar methods
static int LF_SetMinMaxValues(lua_State* L) { return 0; }
static int LF_GetMinMaxValues(lua_State* L) { lua_pushnumber(L, 0); lua_pushnumber(L, 100); return 2; }
static int LF_SetValue(lua_State* L) { return 0; }
static int LF_GetValue(lua_State* L) { lua_pushnumber(L, 0); return 1; }
static int LF_SetStatusBarTexture(lua_State* L) { return 0; }
static int LF_SetStatusBarColor(lua_State* L) { return 0; }
static int LF_SetOrientation(lua_State* L) { return 0; }

// EditBox methods
static int LF_SetMaxLetters(lua_State* L) { return 0; }
static int LF_SetAutoFocus(lua_State* L) { return 0; }
static int LF_ClearFocus(lua_State* L) { return 0; }
static int LF_SetFocus(lua_State* L) { return 0; }
static int LF_HasFocus(lua_State* L) { lua_pushboolean(L, 0); return 1; }
static int LF_HighlightText(lua_State* L) { return 0; }
static int LF_SetCursorPosition(lua_State* L) { return 0; }
static int LF_SetTextInsets(lua_State* L) { return 0; }
static int LF_SetMultiLine(lua_State* L) { return 0; }
static int LF_SetFontObject(lua_State* L) { return 0; }

// ScrollFrame methods
static int LF_SetScrollChild(lua_State* L) { return 0; }
static int LF_GetScrollChild(lua_State* L) { lua_pushnil(L); return 1; }
static int LF_SetVerticalScroll(lua_State* L) { return 0; }
static int LF_GetVerticalScroll(lua_State* L) { lua_pushnumber(L, 0); return 1; }
static int LF_SetHorizontalScroll(lua_State* L) { return 0; }
static int LF_GetHorizontalScroll(lua_State* L) { lua_pushnumber(L, 0); return 1; }

// Cooldown methods
static int LF_SetCooldown(lua_State* L) { return 0; }

// frame:Raise() / frame:Lower()
static int LF_Raise(lua_State* L) { return 0; }
static int LF_Lower(lua_State* L) { return 0; }

// frame:SetToplevel
static int LF_SetToplevel(lua_State* L) { return 0; }

// frame:RegisterForDrag
static int LF_RegisterForDrag(lua_State* L) { return 0; }

// frame:StartMoving / StopMovingOrSizing
static int LF_StartMoving(lua_State* L) { return 0; }
static int LF_StopMovingOrSizing(lua_State* L) { return 0; }

// ── Metatable Setup ──────────────────────────────────────────────────────────────

static const luaL_Reg FrameMethods[] =
{
	// Core
	{"GetName", LF_GetName},
	{"GetObjectType", LF_GetObjectType},
	{"RegisterEvent", LF_RegisterEvent},
	{"UnregisterEvent", LF_UnregisterEvent},
	{"UnregisterAllEvents", LF_UnregisterAllEvents},
	{"SetScript", LF_SetScript},
	{"GetScript", LF_GetScript},
	{"HookScript", LF_HookScript},
	{"HasScript", LF_HasScript},

	// Visibility
	{"Show", LF_Show},
	{"Hide", LF_Hide},
	{"IsShown", LF_IsShown},
	{"IsVisible", LF_IsVisible},

	// Positioning
	{"SetPoint", LF_SetPoint},
	{"ClearAllPoints", LF_ClearAllPoints},
	{"SetAllPoints", LF_SetAllPoints},
	{"SetSize", LF_SetSize},
	{"SetWidth", LF_SetWidth},
	{"SetHeight", LF_SetHeight},
	{"GetWidth", LF_GetWidth},
	{"GetHeight", LF_GetHeight},
	{"GetCenter", LF_GetCenter},
	{"GetLeft", LF_GetLeft},
	{"GetRight", LF_GetRight},
	{"GetTop", LF_GetTop},
	{"GetBottom", LF_GetBottom},

	// Appearance
	{"SetAlpha", LF_SetAlpha},
	{"GetAlpha", LF_GetAlpha},
	{"SetBackdrop", LF_SetBackdrop},
	{"SetBackdropColor", LF_SetBackdropColor},
	{"SetBackdropBorderColor", LF_SetBackdropBorderColor},

	// Strata/Level
	{"SetFrameStrata", LF_SetFrameStrata},
	{"GetFrameStrata", LF_GetFrameStrata},
	{"SetFrameLevel", LF_SetFrameLevel},
	{"GetFrameLevel", LF_GetFrameLevel},
	{"Raise", LF_Raise},
	{"Lower", LF_Lower},
	{"SetToplevel", LF_SetToplevel},

	// Input
	{"EnableMouse", LF_EnableMouse},
	{"EnableKeyboard", LF_EnableKeyboard},
	{"EnableMouseWheel", LF_EnableMouseWheel},
	{"SetMovable", LF_SetMovable},
	{"SetResizable", LF_SetResizable},
	{"SetClampedToScreen", LF_SetClampedToScreen},
	{"RegisterForDrag", LF_RegisterForDrag},
	{"StartMoving", LF_StartMoving},
	{"StopMovingOrSizing", LF_StopMovingOrSizing},

	// Hierarchy
	{"SetParent", LF_SetParent},
	{"GetParent", LF_GetParent},
	{"GetChildren", LF_GetChildren},
	{"GetNumChildren", LF_GetNumChildren},

	// ID/Attributes
	{"SetID", LF_SetID},
	{"GetID", LF_GetID},
	{"SetAttribute", LF_SetAttribute},
	{"GetAttribute", LF_GetAttribute},

	// Creation
	{"CreateTexture", LF_CreateTexture},
	{"CreateFontString", LF_CreateFontString},

	// Button
	{"SetNormalTexture", LF_SetNormalTexture},
	{"SetPushedTexture", LF_SetPushedTexture},
	{"SetHighlightTexture", LF_SetHighlightTexture},
	{"SetDisabledTexture", LF_SetDisabledTexture},
	{"SetText", LF_SetText},
	{"GetText", LF_GetText},
	{"Disable", LF_Disable},
	{"Enable", LF_Enable},
	{"IsEnabled", LF_IsEnabled},
	{"Click", LF_Click},

	// StatusBar
	{"SetMinMaxValues", LF_SetMinMaxValues},
	{"GetMinMaxValues", LF_GetMinMaxValues},
	{"SetValue", LF_SetValue},
	{"GetValue", LF_GetValue},
	{"SetStatusBarTexture", LF_SetStatusBarTexture},
	{"SetStatusBarColor", LF_SetStatusBarColor},
	{"SetOrientation", LF_SetOrientation},

	// EditBox
	{"SetMaxLetters", LF_SetMaxLetters},
	{"SetAutoFocus", LF_SetAutoFocus},
	{"ClearFocus", LF_ClearFocus},
	{"SetFocus", LF_SetFocus},
	{"HasFocus", LF_HasFocus},
	{"HighlightText", LF_HighlightText},
	{"SetCursorPosition", LF_SetCursorPosition},
	{"SetTextInsets", LF_SetTextInsets},
	{"SetMultiLine", LF_SetMultiLine},
	{"SetFontObject", LF_SetFontObject},

	// ScrollFrame
	{"SetScrollChild", LF_SetScrollChild},
	{"GetScrollChild", LF_GetScrollChild},
	{"SetVerticalScroll", LF_SetVerticalScroll},
	{"GetVerticalScroll", LF_GetVerticalScroll},
	{"SetHorizontalScroll", LF_SetHorizontalScroll},
	{"GetHorizontalScroll", LF_GetHorizontalScroll},

	// Cooldown
	{"SetCooldown", LF_SetCooldown},

	{nullptr, nullptr}
};

void WowLuaApi::RegisterFrameApi(lua_State* L)
{
	// Create the WowFrame metatable
	luaL_newmetatable(L, FRAME_METATABLE);

	// Set __index to itself so methods are found via metatable lookup
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	// Register all frame methods
	for (const luaL_Reg* Reg = FrameMethods; Reg->name; Reg++)
	{
		lua_pushcfunction(L, Reg->func);
		lua_setfield(L, -2, Reg->name);
	}

	lua_pop(L, 1); // pop metatable

	// Register global CreateFrame
	lua_register(L, "CreateFrame", L_CreateFrame);

	UE_LOG(LogWowLuaFrame, Log, TEXT("Registered WoW Frame API (CreateFrame + %d methods)"),
		(int)(sizeof(FrameMethods) / sizeof(FrameMethods[0]) - 1));
}

#else
void WowLuaApi::RegisterFrameApi(lua_State*) {}
#endif
