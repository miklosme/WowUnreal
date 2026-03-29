#include "LuaApiRegistry.h"
#include "WowEventSystem.h"
#include "WowFrameManager.h"
#include "WowFrameTypes.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
#include "Slate/WidgetTransform.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Slate/SlateBrushAsset.h"

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
			case EWowFrameType::CheckButton: lua_pushstring(L, "CheckButton"); break;
			case EWowFrameType::EditBox: lua_pushstring(L, "EditBox"); break;
			case EWowFrameType::ScrollFrame: lua_pushstring(L, "ScrollFrame"); break;
			case EWowFrameType::Slider: lua_pushstring(L, "Slider"); break;
			case EWowFrameType::StatusBar: lua_pushstring(L, "StatusBar"); break;
			case EWowFrameType::SimpleHTML: lua_pushstring(L, "SimpleHTML"); break;
			case EWowFrameType::Cooldown: lua_pushstring(L, "Cooldown"); break;
			case EWowFrameType::GameTooltip: lua_pushstring(L, "GameTooltip"); break;
			case EWowFrameType::Minimap: lua_pushstring(L, "Minimap"); break;
			case EWowFrameType::Model: lua_pushstring(L, "Model"); break;
			case EWowFrameType::PlayerModel: lua_pushstring(L, "PlayerModel"); break;
			case EWowFrameType::MessageFrame: lua_pushstring(L, "MessageFrame"); break;
			case EWowFrameType::ScrollingMessageFrame: lua_pushstring(L, "ScrollingMessageFrame"); break;
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
			// Store the Lua function reference directly in the event system
			lua_pushvalue(L, 3);
			int32 Ref = luaL_ref(L, LUA_REGISTRYINDEX);
			Ctx->EventSystem->SetFrameScriptRef(Handle, UTF8_TO_TCHAR(ScriptType), Ref);

			// Also store in a global key so GetScript can retrieve it
			FString GlobalKey = FString::Printf(TEXT("__WowScript_%lld_%s"),
				Handle, UTF8_TO_TCHAR(ScriptType));
			FTCHARToUTF8 UTF8GlobalKey(*GlobalKey);
			lua_pushvalue(L, 3);
			lua_setglobal(L, UTF8GlobalKey.Get());
		}
		else if (lua_isstring(L, 3))
		{
			// Code string passed - compile it
			const char* CodeStr = lua_tostring(L, 3);
			Ctx->EventSystem->SetFrameScript(Handle, UTF8_TO_TCHAR(ScriptType), UTF8_TO_TCHAR(CodeStr));
		}
		else if (lua_isnil(L, 3))
		{
			// Clear the script
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
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		lua_pushboolean(L, Ctx->FrameManager->IsFrameVisible(Handle) ? 1 : 0);
		return 1;
	}
	lua_pushboolean(L, 1);
	return 1;
}

// frame:IsVisible()
static int LF_IsVisible(lua_State* L)
{
	// IsVisible checks both self and parent chain — for now same as IsShown
	return LF_IsShown(L);
}

// Parse a WoW anchor point string to enum
static EWowAnchorPoint ParseAnchorPoint(const char* Str)
{
	if (!Str) return EWowAnchorPoint::CENTER;
	if (strcmp(Str, "TOPLEFT") == 0) return EWowAnchorPoint::TOPLEFT;
	if (strcmp(Str, "TOP") == 0) return EWowAnchorPoint::TOP;
	if (strcmp(Str, "TOPRIGHT") == 0) return EWowAnchorPoint::TOPRIGHT;
	if (strcmp(Str, "LEFT") == 0) return EWowAnchorPoint::LEFT;
	if (strcmp(Str, "CENTER") == 0) return EWowAnchorPoint::CENTER;
	if (strcmp(Str, "RIGHT") == 0) return EWowAnchorPoint::RIGHT;
	if (strcmp(Str, "BOTTOMLEFT") == 0) return EWowAnchorPoint::BOTTOMLEFT;
	if (strcmp(Str, "BOTTOM") == 0) return EWowAnchorPoint::BOTTOM;
	if (strcmp(Str, "BOTTOMRIGHT") == 0) return EWowAnchorPoint::BOTTOMRIGHT;
	return EWowAnchorPoint::CENTER;
}

// Convert anchor point enum to string
static const char* AnchorPointToString(EWowAnchorPoint Point)
{
	switch (Point)
	{
	case EWowAnchorPoint::TOPLEFT: return "TOPLEFT";
	case EWowAnchorPoint::TOP: return "TOP";
	case EWowAnchorPoint::TOPRIGHT: return "TOPRIGHT";
	case EWowAnchorPoint::LEFT: return "LEFT";
	case EWowAnchorPoint::CENTER: return "CENTER";
	case EWowAnchorPoint::RIGHT: return "RIGHT";
	case EWowAnchorPoint::BOTTOMLEFT: return "BOTTOMLEFT";
	case EWowAnchorPoint::BOTTOM: return "BOTTOM";
	case EWowAnchorPoint::BOTTOMRIGHT: return "BOTTOMRIGHT";
	default: return "CENTER";
	}
}

// frame:SetPoint(point, relativeTo, relativePoint, ofsx, ofsy)
static int LF_SetPoint(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (!Ctx || !Ctx->FrameManager) return 0;

	const char* PointStr = luaL_checkstring(L, 2);
	FWowAnchor Anchor;
	Anchor.Point = ParseAnchorPoint(PointStr);

	// arg3 = relativeTo (frame table, frame name string, or nil)
	if (lua_istable(L, 3))
	{
		lua_getfield(L, 3, "__name");
		if (lua_isstring(L, -1))
			Anchor.RelativeTo = UTF8_TO_TCHAR(lua_tostring(L, -1));
		lua_pop(L, 1);

		// When relativeTo is a frame, arg4 = relativePoint, arg5 = x, arg6 = y
		Anchor.RelativePoint = ParseAnchorPoint(luaL_optstring(L, 4, PointStr));
		Anchor.OffsetX = static_cast<float>(luaL_optnumber(L, 5, 0));
		Anchor.OffsetY = static_cast<float>(luaL_optnumber(L, 6, 0));
	}
	else if (lua_isstring(L, 3))
	{
		// Could be relativeTo name or relativePoint if no frame given
		const char* Arg3 = lua_tostring(L, 3);
		// Check if arg3 looks like an anchor point name
		if (strcmp(Arg3, "TOPLEFT") == 0 || strcmp(Arg3, "TOP") == 0 || strcmp(Arg3, "TOPRIGHT") == 0 ||
			strcmp(Arg3, "LEFT") == 0 || strcmp(Arg3, "CENTER") == 0 || strcmp(Arg3, "RIGHT") == 0 ||
			strcmp(Arg3, "BOTTOMLEFT") == 0 || strcmp(Arg3, "BOTTOM") == 0 || strcmp(Arg3, "BOTTOMRIGHT") == 0)
		{
			// SetPoint("POINT", "RELATIVEPOINT", x, y) — no relativeTo frame
			Anchor.RelativePoint = ParseAnchorPoint(Arg3);
			Anchor.OffsetX = static_cast<float>(luaL_optnumber(L, 4, 0));
			Anchor.OffsetY = static_cast<float>(luaL_optnumber(L, 5, 0));
		}
		else
		{
			// SetPoint("POINT", "frameName", "RELATIVEPOINT", x, y)
			Anchor.RelativeTo = UTF8_TO_TCHAR(Arg3);
			Anchor.RelativePoint = ParseAnchorPoint(luaL_optstring(L, 4, PointStr));
			Anchor.OffsetX = static_cast<float>(luaL_optnumber(L, 5, 0));
			Anchor.OffsetY = static_cast<float>(luaL_optnumber(L, 6, 0));
		}
	}
	else
	{
		// SetPoint("POINT", nil, nil, x, y) or SetPoint("POINT", x, y)
		Anchor.RelativePoint = Anchor.Point;
		if (lua_isnumber(L, 3))
		{
			Anchor.OffsetX = static_cast<float>(lua_tonumber(L, 3));
			Anchor.OffsetY = static_cast<float>(luaL_optnumber(L, 4, 0));
		}
		else
		{
			Anchor.OffsetX = static_cast<float>(luaL_optnumber(L, 5, 0));
			Anchor.OffsetY = static_cast<float>(luaL_optnumber(L, 6, 0));
		}
	}

	// Replace existing anchor with same point, or append if new
	FWowFrameDef* Def = Ctx->FrameManager->GetMutableFrameDef(Handle);
	if (Def)
	{
		bool bReplaced = false;
		for (int32 i = 0; i < Def->Anchors.Num(); i++)
		{
			if (Def->Anchors[i].Point == Anchor.Point)
			{
				Def->Anchors[i] = Anchor;
				bReplaced = true;
				break;
			}
		}
		if (!bReplaced)
		{
			Def->Anchors.Add(Anchor);
		}
		Ctx->FrameManager->SetFrameAnchors(Handle, Def->Anchors);
	}
	return 0;
}

// frame:ClearAllPoints()
static int LF_ClearAllPoints(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		Ctx->FrameManager->ClearFrameAnchors(Handle);
	}
	return 0;
}

// frame:SetAllPoints(relativeTo)
static int LF_SetAllPoints(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		FWowFrameDef* Def = Ctx->FrameManager->GetMutableFrameDef(Handle);
		if (Def)
		{
			Def->bSetAllPoints = true;
			Def->Anchors.Empty();
			Ctx->FrameManager->SetFrameAnchors(Handle, Def->Anchors);
		}
	}
	return 0;
}

// frame:SetSize(width, height)
static int LF_SetSize(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		float W = static_cast<float>(luaL_checknumber(L, 2));
		float H = static_cast<float>(luaL_checknumber(L, 3));
		Ctx->FrameManager->SetFrameSize(Handle, W, H);
	}
	return 0;
}

// frame:SetWidth(width)
static int LF_SetWidth(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		float W = static_cast<float>(luaL_checknumber(L, 2));
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		float H = Def ? Def->Height : 0.f;
		Ctx->FrameManager->SetFrameSize(Handle, W, H);
	}
	return 0;
}

// frame:SetHeight(height)
static int LF_SetHeight(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		float H = static_cast<float>(luaL_checknumber(L, 2));
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		float W = Def ? Def->Width : 0.f;
		Ctx->FrameManager->SetFrameSize(Handle, W, H);
	}
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
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		float Alpha = static_cast<float>(luaL_checknumber(L, 2));
		Ctx->FrameManager->SetFrameAlpha(Handle, Alpha);
		// Store alpha on frame table for GetAlpha
		lua_pushnumber(L, Alpha);
		lua_setfield(L, 1, "__alpha");
	}
	return 0;
}

// frame:GetAlpha()
static int LF_GetAlpha(lua_State* L)
{
	lua_getfield(L, 1, "__alpha");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushnumber(L, 1.0);
	}
	return 1;
}

// Parse strata string
static EWowFrameStrata ParseFrameStrata(const char* Str)
{
	if (!Str) return EWowFrameStrata::MEDIUM;
	if (strcmp(Str, "BACKGROUND") == 0) return EWowFrameStrata::BACKGROUND;
	if (strcmp(Str, "LOW") == 0) return EWowFrameStrata::LOW;
	if (strcmp(Str, "MEDIUM") == 0) return EWowFrameStrata::MEDIUM;
	if (strcmp(Str, "HIGH") == 0) return EWowFrameStrata::HIGH;
	if (strcmp(Str, "DIALOG") == 0) return EWowFrameStrata::DIALOG;
	if (strcmp(Str, "FULLSCREEN") == 0) return EWowFrameStrata::FULLSCREEN;
	if (strcmp(Str, "FULLSCREEN_DIALOG") == 0) return EWowFrameStrata::FULLSCREEN_DIALOG;
	if (strcmp(Str, "TOOLTIP") == 0) return EWowFrameStrata::TOOLTIP;
	return EWowFrameStrata::MEDIUM;
}

static const char* StrataToString(EWowFrameStrata S)
{
	switch (S)
	{
	case EWowFrameStrata::BACKGROUND: return "BACKGROUND";
	case EWowFrameStrata::LOW: return "LOW";
	case EWowFrameStrata::MEDIUM: return "MEDIUM";
	case EWowFrameStrata::HIGH: return "HIGH";
	case EWowFrameStrata::DIALOG: return "DIALOG";
	case EWowFrameStrata::FULLSCREEN: return "FULLSCREEN";
	case EWowFrameStrata::FULLSCREEN_DIALOG: return "FULLSCREEN_DIALOG";
	case EWowFrameStrata::TOOLTIP: return "TOOLTIP";
	default: return "MEDIUM";
	}
}

// frame:SetFrameStrata(strata)
static int LF_SetFrameStrata(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const char* StrataStr = luaL_checkstring(L, 2);
		Ctx->FrameManager->SetFrameStrata(Handle, ParseFrameStrata(StrataStr));
	}
	return 0;
}

// frame:GetFrameStrata()
static int LF_GetFrameStrata(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		if (Def)
		{
			lua_pushstring(L, StrataToString(Def->Strata));
			return 1;
		}
	}
	lua_pushstring(L, "MEDIUM");
	return 1;
}

// frame:SetFrameLevel(level)
static int LF_SetFrameLevel(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		int32 Level = static_cast<int32>(luaL_checknumber(L, 2));
		Ctx->FrameManager->SetFrameLevel(Handle, Level);
	}
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
	// TODO: reparenting requires moving widget between canvases
	return 0;
}

// frame:GetParent()
static int LF_GetParent(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 ParentHandle = Ctx->FrameManager->GetParentHandle(Handle);
		if (ParentHandle >= 0)
		{
			FString ParentName = Ctx->FrameManager->GetFrameName(ParentHandle);
			if (!ParentName.IsEmpty())
			{
				lua_getglobal(L, TCHAR_TO_UTF8(*ParentName));
				if (lua_istable(L, -1))
					return 1;
				lua_pop(L, 1);
			}
		}
	}
	lua_pushnil(L);
	return 1;
}

// frame:GetChildren()
static int LF_GetChildren(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		TArray<int64> Children = Ctx->FrameManager->GetChildHandles(Handle);
		int Count = 0;
		for (int64 ChildHandle : Children)
		{
			FString ChildName = Ctx->FrameManager->GetFrameName(ChildHandle);
			if (!ChildName.IsEmpty())
			{
				lua_getglobal(L, TCHAR_TO_UTF8(*ChildName));
				if (lua_istable(L, -1))
				{
					Count++;
				}
				else
				{
					lua_pop(L, 1);
				}
			}
		}
		return Count;
	}
	return 0;
}

// frame:GetNumChildren()
static int LF_GetNumChildren(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		TArray<int64> Children = Ctx->FrameManager->GetChildHandles(Handle);
		lua_pushnumber(L, Children.Num());
		return 1;
	}
	lua_pushnumber(L, 0);
	return 1;
}

// Helper: get the UImage* from a texture Lua table (either named or dynamic)
static UImage* GetTextureImage(lua_State* L, int idx = 1)
{
	// Try dynamic image widget first (from CreateTexture + SetTexture)
	lua_getfield(L, idx, "__imageWidget");
	if (lua_islightuserdata(L, -1))
	{
		UImage* Img = static_cast<UImage*>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return Img;
	}
	lua_pop(L, 1);

	// Try named texture region (from XML <Texture name="...">)
	lua_getfield(L, idx, "__name");
	if (lua_isstring(L, -1))
	{
		FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
		if (Ctx && Ctx->FrameManager)
		{
			UImage* Img = Ctx->FrameManager->GetNamedTexture(UTF8_TO_TCHAR(lua_tostring(L, -1)));
			lua_pop(L, 1);
			return Img;
		}
	}
	lua_pop(L, 1);

	return nullptr;
}

// Texture region methods
static int LF_TextureSetTexture(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	// WoW allows SetTexture(nil) to clear the texture
	if (lua_isnil(L, 2) || lua_isnone(L, 2))
	{
		lua_pushnil(L);
		lua_setfield(L, 1, "__texture");
		return 0;
	}
	// Also handle number arg (texture ID) — convert to string
	const char* TexturePath;
	if (lua_isnumber(L, 2))
	{
		lua_pushfstring(L, "%d", (int)lua_tonumber(L, 2));
		TexturePath = lua_tostring(L, -1);
		lua_pop(L, 1);
	}
	else
	{
		TexturePath = luaL_checkstring(L, 2);
	}

	// Store in Lua table
	lua_pushstring(L, TexturePath);
	lua_setfield(L, 1, "__texture");

	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		// First, try to find this texture region by name (from XML Texture name="Foo")
		lua_getfield(L, 1, "__name");
		const char* TexName = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
		lua_pop(L, 1);

		UImage* NamedImage = TexName ? Ctx->FrameManager->GetNamedTexture(UTF8_TO_TCHAR(TexName)) : nullptr;

		UTexture2D* Texture = Ctx->FrameManager->LoadUITexture(UTF8_TO_TCHAR(TexturePath));
		if (Texture && NamedImage)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
			Brush.Tiling = ESlateBrushTileType::NoTile;

			// Apply stored UV coordinates if any, handling WoW's Left>Right flip convention
			lua_getfield(L, 1, "__texLeft");
			if (lua_isnumber(L, -1))
			{
				float UVLeft = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
				lua_getfield(L, 1, "__texRight"); float UVRight = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
				lua_getfield(L, 1, "__texTop"); float UVTop = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
				lua_getfield(L, 1, "__texBottom"); float UVBottom = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);

				float NL = FMath::Min(UVLeft, UVRight), NR = FMath::Max(UVLeft, UVRight);
				float NT = FMath::Min(UVTop, UVBottom), NB = FMath::Max(UVTop, UVBottom);
				FBox2D UVRegion(FVector2D(NL, NT), FVector2D(NR, NB));
				Brush.SetUVRegion(UVRegion);

				bool bFlipH = UVLeft > UVRight;
				bool bFlipV = UVTop > UVBottom;
				if (bFlipH || bFlipV)
				{
					FVector2D Scale(bFlipH ? -1.0f : 1.0f, bFlipV ? -1.0f : 1.0f);
					NamedImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
					NamedImage->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, Scale, FVector2D::ZeroVector, 0.0f));
				}
			}
			else
			{
				lua_pop(L, 1);
			}

			NamedImage->SetBrush(Brush);
			NamedImage->SetRenderOpacity(1.0f); // Make visible (may have been hidden as empty placeholder)
			return 0;
		}

		// Fallback: try parent handle approach (dynamic textures from CreateTexture)
		lua_getfield(L, 1, "__parentHandle");
		if (lua_isnumber(L, -1))
		{
			int64 ParentHandle = static_cast<int64>(lua_tointeger(L, -1));
			UWidget* ParentWidget = Ctx->FrameManager->GetWidgetForHandle(ParentHandle);

			if (!Texture)
				Texture = Ctx->FrameManager->LoadUITexture(UTF8_TO_TCHAR(TexturePath));
			if (Texture)
			{
				FSlateBrush Brush;
				Brush.SetResourceObject(Texture);
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Brush.Tiling = ESlateBrushTileType::NoTile;

				// Apply stored UV coordinates if any, handling WoW flip convention
				bool bDynFlipH = false, bDynFlipV = false;
				lua_getfield(L, 1, "__texLeft");
				if (lua_isnumber(L, -1))
				{
					float UVLeft = static_cast<float>(lua_tonumber(L, -1));
					lua_pop(L, 1);
					lua_getfield(L, 1, "__texRight"); float UVRight = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
					lua_getfield(L, 1, "__texTop"); float UVTop = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
					lua_getfield(L, 1, "__texBottom"); float UVBottom = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);

					bDynFlipH = UVLeft > UVRight;
					bDynFlipV = UVTop > UVBottom;
					float NL = FMath::Min(UVLeft, UVRight), NR = FMath::Max(UVLeft, UVRight);
					float NT = FMath::Min(UVTop, UVBottom), NB = FMath::Max(UVTop, UVBottom);
					FBox2D UVRegion(FVector2D(NL, NT), FVector2D(NR, NB));
					Brush.SetUVRegion(UVRegion);
				}
				else
				{
					lua_pop(L, 1);
				}

				if (UButton* Button = Cast<UButton>(ParentWidget))
				{
					FButtonStyle ButtonStyle = Button->GetStyle();
					ButtonStyle.Normal = Brush;
					Button->SetStyle(ButtonStyle);
				}
				else if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(ParentWidget))
				{
					// Check if we already have an image widget for this texture
					UImage* Img = nullptr;
					lua_getfield(L, 1, "__imageWidget");
					if (lua_islightuserdata(L, -1))
					{
						Img = static_cast<UImage*>(lua_touserdata(L, -1));
					}
					lua_pop(L, 1);

					if (!Img)
					{
						Img = NewObject<UImage>(Canvas);
						UCanvasPanelSlot* ImgSlot = Canvas->AddChildToCanvas(Img);
						if (ImgSlot)
						{
							// Check if SetAllPoints was called
							lua_getfield(L, 1, "__setAllPoints");
							bool bFillParent = lua_toboolean(L, -1) != 0;
							lua_pop(L, 1);

							if (bFillParent)
							{
								ImgSlot->SetAnchors(FAnchors(0, 0, 1, 1));
								ImgSlot->SetOffsets(FMargin(0));
							}
							else
							{
								// Check for stored size
								lua_getfield(L, 1, "__width");
								float W = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.f;
								lua_pop(L, 1);
								lua_getfield(L, 1, "__height");
								float H = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.f;
								lua_pop(L, 1);

								float UIScale = Ctx->FrameManager->GetUIScale();
								ImgSlot->SetAnchors(FAnchors(0.0f, 0.0f));
								ImgSlot->SetAlignment(FVector2D(0.0f, 0.0f));

								if (W > 0.f && H > 0.f)
								{
									ImgSlot->SetSize(FVector2D(W * UIScale, H * UIScale));
								}
								else
								{
									// Use texture natural size as default
									ImgSlot->SetSize(FVector2D(Texture->GetSizeX() * UIScale, Texture->GetSizeY() * UIScale));
								}

								// Apply stored position
								lua_getfield(L, 1, "__posX");
								float PosX = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.f;
								lua_pop(L, 1);
								lua_getfield(L, 1, "__posY");
								float PosY = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.f;
								lua_pop(L, 1);
								ImgSlot->SetPosition(FVector2D(PosX * UIScale, PosY * UIScale));
							}
						}

						// Store image widget reference
						lua_pushlightuserdata(L, Img);
						lua_setfield(L, 1, "__imageWidget");
					}

					Img->SetBrush(Brush);

					// Apply stored vertex color
					lua_getfield(L, 1, "__colorR");
					if (lua_isnumber(L, -1))
					{
						float R = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
						lua_getfield(L, 1, "__colorG"); float G = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
						lua_getfield(L, 1, "__colorB"); float B = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
						lua_getfield(L, 1, "__colorA"); float A = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 1.0f; lua_pop(L, 1);
						Img->SetColorAndOpacity(FLinearColor(R, G, B, A));
					}
					else
					{
						lua_pop(L, 1);
					}

					// Apply stored alpha
					lua_getfield(L, 1, "__alpha");
					if (lua_isnumber(L, -1))
					{
						float Alpha = static_cast<float>(lua_tonumber(L, -1));
						Img->SetRenderOpacity(Alpha);
					}
					lua_pop(L, 1);
				}
			}
		}
		lua_pop(L, 1);
	}

	return 0;
}

static int LF_TextureSetTexCoord(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float Left = static_cast<float>(luaL_checknumber(L, 2));
	float Right = static_cast<float>(luaL_checknumber(L, 3));
	float Top = static_cast<float>(luaL_checknumber(L, 4));
	float Bottom = static_cast<float>(luaL_checknumber(L, 5));

	// Store in Lua table
	lua_pushnumber(L, Left); lua_setfield(L, 1, "__texLeft");
	lua_pushnumber(L, Right); lua_setfield(L, 1, "__texRight");
	lua_pushnumber(L, Top); lua_setfield(L, 1, "__texTop");
	lua_pushnumber(L, Bottom); lua_setfield(L, 1, "__texBottom");

	// Apply UV region to the UImage widget
	// WoW uses Left > Right for horizontal flip and Top > Bottom for vertical flip.
	// Normalize UVs for FBox2D and apply render transform for flipping.
	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		float UVL = FMath::Min(Left, Right);
		float UVR = FMath::Max(Left, Right);
		float UVT = FMath::Min(Top, Bottom);
		float UVB = FMath::Max(Top, Bottom);

		FSlateBrush Brush = Img->GetBrush();
		FBox2D UVRegion(FVector2D(UVL, UVT), FVector2D(UVR, UVB));
		Brush.SetUVRegion(UVRegion);
		Img->SetBrush(Brush);

		bool bFlipH = Left > Right;
		bool bFlipV = Top > Bottom;
		if (bFlipH || bFlipV)
		{
			FVector2D Scale(bFlipH ? -1.0f : 1.0f, bFlipV ? -1.0f : 1.0f);
			Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Img->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, Scale, FVector2D::ZeroVector, 0.0f));
		}
		else
		{
			// Clear any previous flip transform
			Img->SetRenderTransform(FWidgetTransform());
		}
	}

	return 0;
}

static int LF_TextureSetVertexColor(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float R = static_cast<float>(luaL_checknumber(L, 2));
	float G = static_cast<float>(luaL_checknumber(L, 3));
	float B = static_cast<float>(luaL_checknumber(L, 4));
	float A = lua_isnumber(L, 5) ? static_cast<float>(lua_tonumber(L, 5)) : 1.0f;

	// Store in Lua table
	lua_pushnumber(L, R); lua_setfield(L, 1, "__colorR");
	lua_pushnumber(L, G); lua_setfield(L, 1, "__colorG");
	lua_pushnumber(L, B); lua_setfield(L, 1, "__colorB");
	lua_pushnumber(L, A); lua_setfield(L, 1, "__colorA");

	// Apply color tinting to the texture
	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		Img->SetColorAndOpacity(FLinearColor(R, G, B, A));
	}

	return 0;
}

static int LF_TextureSetAlpha(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float Alpha = static_cast<float>(luaL_checknumber(L, 2));

	// Store in Lua table
	lua_pushnumber(L, Alpha);
	lua_setfield(L, 1, "__alpha");

	// Apply alpha to the texture region
	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		Img->SetRenderOpacity(Alpha);
	}

	return 0;
}

// Dynamic texture: Show
static int LF_TextureShow(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		Img->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return 0;
}

// Dynamic texture: Hide
static int LF_TextureHide(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		Img->SetVisibility(ESlateVisibility::Collapsed);
	}
	return 0;
}

// Dynamic texture: SetSize
static int LF_TextureSetSize(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float W = static_cast<float>(luaL_checknumber(L, 2));
	float H = static_cast<float>(luaL_checknumber(L, 3));

	lua_pushnumber(L, W); lua_setfield(L, 1, "__width");
	lua_pushnumber(L, H); lua_setfield(L, 1, "__height");

	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Img->Slot);
		if (Slot)
		{
			FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
			float UIScale = (Ctx && Ctx->FrameManager) ? Ctx->FrameManager->GetUIScale() : 1.0f;
			Slot->SetSize(FVector2D(W * UIScale, H * UIScale));
		}
	}
	return 0;
}

// Dynamic texture: SetWidth
static int LF_TextureSetWidth(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float W = static_cast<float>(luaL_checknumber(L, 2));
	lua_pushnumber(L, W); lua_setfield(L, 1, "__width");

	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Img->Slot);
		if (Slot)
		{
			FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
			float UIScale = (Ctx && Ctx->FrameManager) ? Ctx->FrameManager->GetUIScale() : 1.0f;
			FVector2D Size = Slot->GetSize();
			Slot->SetSize(FVector2D(W * UIScale, Size.Y));
		}
	}
	return 0;
}

// Dynamic texture: SetHeight
static int LF_TextureSetHeight(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float H = static_cast<float>(luaL_checknumber(L, 2));
	lua_pushnumber(L, H); lua_setfield(L, 1, "__height");

	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Img->Slot);
		if (Slot)
		{
			FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
			float UIScale = (Ctx && Ctx->FrameManager) ? Ctx->FrameManager->GetUIScale() : 1.0f;
			FVector2D Size = Slot->GetSize();
			Slot->SetSize(FVector2D(Size.X, H * UIScale));
		}
	}
	return 0;
}

// Dynamic texture: GetWidth
static int LF_TextureGetWidth(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "__width");
	if (!lua_isnumber(L, -1))
	{
		lua_pop(L, 1);
		lua_pushnumber(L, 0);
	}
	return 1;
}

// Dynamic texture: GetHeight
static int LF_TextureGetHeight(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "__height");
	if (!lua_isnumber(L, -1))
	{
		lua_pop(L, 1);
		lua_pushnumber(L, 0);
	}
	return 1;
}

// Dynamic texture: SetAllPoints — flag for filling parent
static int LF_TextureSetAllPoints(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushboolean(L, 1);
	lua_setfield(L, 1, "__setAllPoints");

	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Img->Slot);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			Slot->SetOffsets(FMargin(0));
		}
	}
	return 0;
}

// Dynamic texture: SetPoint — stores position for when SetTexture creates the image
static int LF_TextureSetPoint(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	// Simplified: just store offset X/Y for positioning
	// Full anchor system would need the parent reference resolution
	int nargs = lua_gettop(L);
	float offX = 0.f, offY = 0.f;

	if (nargs >= 5 && lua_isnumber(L, 4))
	{
		offX = static_cast<float>(lua_tonumber(L, 4));
		offY = static_cast<float>(lua_tonumber(L, 5));
	}
	else if (nargs >= 4 && lua_isnumber(L, 3))
	{
		offX = static_cast<float>(lua_tonumber(L, 3));
		offY = static_cast<float>(lua_tonumber(L, 4));
	}

	lua_pushnumber(L, offX); lua_setfield(L, 1, "__posX");
	lua_pushnumber(L, offY); lua_setfield(L, 1, "__posY");

	UImage* Img = GetTextureImage(L, 1);
	if (Img)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Img->Slot);
		if (Slot)
		{
			FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
			float UIScale = (Ctx && Ctx->FrameManager) ? Ctx->FrameManager->GetUIScale() : 1.0f;
			Slot->SetPosition(FVector2D(offX * UIScale, offY * UIScale));
		}
	}
	return 0;
}

// frame:CreateTexture(name, layer, inherits)
static int LF_CreateTexture(lua_State* L)
{
	// Create a minimal texture table with a metatable
	lua_newtable(L);

	// Store parent frame handle for texture regions
	int64 ParentHandle = GetFrameHandle(L);
	lua_pushinteger(L, ParentHandle);
	lua_setfield(L, -2, "__parentHandle");

	// Store name if provided
	const char* Name = luaL_optstring(L, 2, nullptr);
	if (Name && Name[0])
	{
		lua_pushstring(L, Name);
		lua_setfield(L, -2, "__name");
	}

	// Texture methods
	lua_pushcfunction(L, LF_TextureSetTexture);
	lua_setfield(L, -2, "SetTexture");
	lua_pushcfunction(L, LF_TextureSetTexCoord);
	lua_setfield(L, -2, "SetTexCoord");
	lua_pushcfunction(L, LF_TextureSetVertexColor);
	lua_setfield(L, -2, "SetVertexColor");
	lua_pushcfunction(L, LF_TextureSetAlpha);
	lua_setfield(L, -2, "SetAlpha");
	lua_pushcfunction(L, LF_TextureShow);
	lua_setfield(L, -2, "Show");
	lua_pushcfunction(L, LF_TextureHide);
	lua_setfield(L, -2, "Hide");
	lua_pushcfunction(L, LF_TextureSetPoint);
	lua_setfield(L, -2, "SetPoint");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // ClearAllPoints
	lua_setfield(L, -2, "ClearAllPoints");
	lua_pushcfunction(L, LF_TextureSetAllPoints);
	lua_setfield(L, -2, "SetAllPoints");
	lua_pushcfunction(L, LF_TextureSetSize);
	lua_setfield(L, -2, "SetSize");
	lua_pushcfunction(L, LF_TextureSetWidth);
	lua_setfield(L, -2, "SetWidth");
	lua_pushcfunction(L, LF_TextureSetHeight);
	lua_setfield(L, -2, "SetHeight");
	lua_pushcfunction(L, LF_TextureGetWidth);
	lua_setfield(L, -2, "GetWidth");
	lua_pushcfunction(L, LF_TextureGetHeight);
	lua_setfield(L, -2, "GetHeight");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushboolean(L2, 1); return 1; }); // IsShown
	lua_setfield(L, -2, "IsShown");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushstring(L2, "Texture"); return 1; }); // GetObjectType
	lua_setfield(L, -2, "GetObjectType");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetBlendMode
	lua_setfield(L, -2, "SetBlendMode");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetDesaturated
	lua_setfield(L, -2, "SetDesaturated");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetNonBlocking
	lua_setfield(L, -2, "SetNonBlocking");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetDrawLayer
	lua_setfield(L, -2, "SetDrawLayer");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetGradient
	lua_setfield(L, -2, "SetGradient");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetGradientAlpha
	lua_setfield(L, -2, "SetGradientAlpha");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetRotation
	lua_setfield(L, -2, "SetRotation");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetTexCoordModifiesRect
	lua_setfield(L, -2, "SetTexCoordModifiesRect");
	lua_pushcfunction(L, [](lua_State* L2) -> int { lua_pushnumber(L2, 0); lua_pushnumber(L2, 0); lua_pushnumber(L2, 1); lua_pushnumber(L2, 1); return 4; }); // GetTexCoord
	lua_setfield(L, -2, "GetTexCoord");

	return 1;
}

// FontString methods
static int LF_FontStringSetText(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	const char* Text = luaL_optstring(L, 2, ""); // WoW treats nil as empty string

	// Store in Lua table
	lua_pushstring(L, Text);
	lua_setfield(L, 1, "__text");

	// For FontString regions, we need to get the parent frame handle
	// and find the corresponding UTextBlock widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		lua_getfield(L, 1, "__parentHandle");
		if (lua_isnumber(L, -1))
		{
			int64 ParentHandle = static_cast<int64>(lua_tointeger(L, -1));
			UWidget* ParentWidget = Ctx->FrameManager->GetWidgetForHandle(ParentHandle);

			// TODO: Proper FontString widget handling
			// For now, try to cast parent to TextBlock
			if (UTextBlock* TextBlock = Cast<UTextBlock>(ParentWidget))
			{
				FText TextContent = FText::FromString(UTF8_TO_TCHAR(Text));
				TextBlock->SetText(TextContent);
			}
		}
		lua_pop(L, 1);
	}

	return 0;
}

static int LF_FontStringGetText(lua_State* L)
{
	lua_getfield(L, 1, "__text");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushstring(L, "");
	}
	return 1;
}

static int LF_FontStringSetTextColor(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float R = static_cast<float>(luaL_checknumber(L, 2));
	float G = static_cast<float>(luaL_checknumber(L, 3));
	float B = static_cast<float>(luaL_checknumber(L, 4));
	float A = lua_isnumber(L, 5) ? static_cast<float>(lua_tonumber(L, 5)) : 1.0f;

	// Store in Lua table
	lua_pushnumber(L, R); lua_setfield(L, 1, "__textColorR");
	lua_pushnumber(L, G); lua_setfield(L, 1, "__textColorG");
	lua_pushnumber(L, B); lua_setfield(L, 1, "__textColorB");
	lua_pushnumber(L, A); lua_setfield(L, 1, "__textColorA");

	// Apply to widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		lua_getfield(L, 1, "__parentHandle");
		if (lua_isnumber(L, -1))
		{
			int64 ParentHandle = static_cast<int64>(lua_tointeger(L, -1));
			UWidget* ParentWidget = Ctx->FrameManager->GetWidgetForHandle(ParentHandle);

			if (UTextBlock* TextBlock = Cast<UTextBlock>(ParentWidget))
			{
				FLinearColor TextColor(R, G, B, A);
				TextBlock->SetColorAndOpacity(FSlateColor(TextColor));
			}
		}
		lua_pop(L, 1);
	}

	return 0;
}

// fontstring:SetFont(fontPath, size, flags)
static int LF_FontStringSetFont(lua_State* L)
{
	// Store font info in the Lua table for later retrieval
	const char* FontPath = luaL_optstring(L, 2, "Fonts\\FRIZQT__.TTF");
	float FontSize = static_cast<float>(luaL_optnumber(L, 3, 12.0));
	const char* Flags = luaL_optstring(L, 4, "");

	lua_pushstring(L, FontPath);
	lua_setfield(L, 1, "__fontPath");
	lua_pushnumber(L, FontSize);
	lua_setfield(L, 1, "__fontSize");
	lua_pushstring(L, Flags);
	lua_setfield(L, 1, "__fontFlags");

	return 0;
}

// fontstring:GetFont() -> fontPath, size, flags
static int LF_FontStringGetFont(lua_State* L)
{
	lua_getfield(L, 1, "__fontPath");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushstring(L, "Fonts\\FRIZQT__.TTF");
	}

	lua_getfield(L, 1, "__fontSize");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushnumber(L, 12.0);
	}

	lua_getfield(L, 1, "__fontFlags");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushstring(L, "");
	}

	return 3;
}

// fontstring:GetStringWidth() -> approximate width
static int LF_FontStringGetStringWidth(lua_State* L)
{
	lua_getfield(L, 1, "__text");
	const char* Text = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
	lua_pop(L, 1);

	lua_getfield(L, 1, "__fontSize");
	float FontSize = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 12.0f;
	lua_pop(L, 1);

	// Rough estimate: average character width is ~0.5 * font height
	float Width = static_cast<float>(strlen(Text)) * FontSize * 0.5f;
	lua_pushnumber(L, Width);
	return 1;
}

// frame:CreateFontString(name, layer, inherits)
static int LF_CreateFontString(lua_State* L)
{
	lua_newtable(L);

	// Store parent frame handle for FontString regions
	int64 ParentHandle = GetFrameHandle(L);
	lua_pushinteger(L, ParentHandle);
	lua_setfield(L, -2, "__parentHandle");

	// FontString methods
	lua_pushcfunction(L, LF_FontStringSetText);
	lua_setfield(L, -2, "SetText");
	lua_pushcfunction(L, LF_FontStringGetText);
	lua_setfield(L, -2, "GetText");
	lua_pushcfunction(L, LF_FontStringSetTextColor);
	lua_setfield(L, -2, "SetTextColor");
	lua_pushcfunction(L, LF_FontStringSetFont); // SetFont
	lua_setfield(L, -2, "SetFont");
	lua_pushcfunction(L, [](lua_State* L2) -> int { return 0; }); // SetFontObject
	lua_setfield(L, -2, "SetFontObject");
	lua_pushcfunction(L, LF_FontStringGetFont); // GetFont
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
	lua_pushcfunction(L, LF_FontStringGetStringWidth); // GetStringWidth
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
	lua_pushcfunction(L, LF_FontStringGetStringWidth); // GetTextWidth (alias for GetStringWidth)
	lua_setfield(L, -2, "GetTextWidth");
	lua_pushcfunction(L, LF_FontStringSetTextColor); // SetVertexColor (alias for SetTextColor for fontstrings)
	lua_setfield(L, -2, "SetVertexColor");

	return 1;
}

// frame:SetBackdrop(backdrop)
static int LF_SetBackdrop(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	// Store the backdrop table for reference
	if (lua_isnil(L, 2))
	{
		// Clear backdrop
		lua_pushnil(L);
		lua_setfield(L, 1, "__backdrop");
	}
	else if (lua_istable(L, 2))
	{
		// Store backdrop table
		lua_pushvalue(L, 2);
		lua_setfield(L, 1, "__backdrop");

		// Basic backdrop implementation - apply background texture
		FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
		if (Ctx && Ctx->FrameManager)
		{
			int64 Handle = GetFrameHandle(L);

			// Get bgFile from backdrop table
			lua_getfield(L, 2, "bgFile");
			if (lua_isstring(L, -1))
			{
				const char* BgFile = lua_tostring(L, -1);
				// TODO: Apply background texture to frame
				// This requires creating or modifying background image widget
			}
			lua_pop(L, 1);
		}
	}

	return 0;
}

// Helper: read a color value from the stack, tolerating tables at any position
static double SafeColorArg(lua_State* L, int idx, double def)
{
	if (lua_isnumber(L, idx)) return lua_tonumber(L, idx);
	if (lua_istable(L, idx)) return def; // skip tables silently
	return def;
}

// frame:SetBackdropColor(r, g, b, a) or frame:SetBackdropColor(colorTable)
static int LF_SetBackdropColor(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_newtable(L);

	if (lua_type(L, 2) == LUA_TTABLE)
	{
		lua_getfield(L, 2, "r"); double r = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "g"); double g = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "b"); double b = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "a"); double a = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0; lua_pop(L, 1);
		lua_pushnumber(L, r); lua_rawseti(L, -2, 1);
		lua_pushnumber(L, g); lua_rawseti(L, -2, 2);
		lua_pushnumber(L, b); lua_rawseti(L, -2, 3);
		lua_pushnumber(L, a); lua_rawseti(L, -2, 4);
	}
	else
	{
		lua_pushnumber(L, SafeColorArg(L, 2, 1.0)); lua_rawseti(L, -2, 1);
		lua_pushnumber(L, SafeColorArg(L, 3, 1.0)); lua_rawseti(L, -2, 2);
		lua_pushnumber(L, SafeColorArg(L, 4, 1.0)); lua_rawseti(L, -2, 3);
		lua_pushnumber(L, SafeColorArg(L, 5, 1.0)); lua_rawseti(L, -2, 4);
	}

	lua_setfield(L, 1, "__backdropColor");
	return 0;
}

// frame:SetBackdropBorderColor(r, g, b, a) or frame:SetBackdropBorderColor(colorTable)
static int LF_SetBackdropBorderColor(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_newtable(L);

	if (lua_type(L, 2) == LUA_TTABLE)
	{
		lua_getfield(L, 2, "r"); double r = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "g"); double g = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "b"); double b = lua_tonumber(L, -1); lua_pop(L, 1);
		lua_getfield(L, 2, "a"); double a = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : 1.0; lua_pop(L, 1);
		lua_pushnumber(L, r); lua_rawseti(L, -2, 1);
		lua_pushnumber(L, g); lua_rawseti(L, -2, 2);
		lua_pushnumber(L, b); lua_rawseti(L, -2, 3);
		lua_pushnumber(L, a); lua_rawseti(L, -2, 4);
	}
	else
	{
		lua_pushnumber(L, SafeColorArg(L, 2, 1.0)); lua_rawseti(L, -2, 1);
		lua_pushnumber(L, SafeColorArg(L, 3, 1.0)); lua_rawseti(L, -2, 2);
		lua_pushnumber(L, SafeColorArg(L, 4, 1.0)); lua_rawseti(L, -2, 3);
		lua_pushnumber(L, SafeColorArg(L, 5, 1.0)); lua_rawseti(L, -2, 4);
	}

	lua_setfield(L, 1, "__backdropBorderColor");
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
	luaL_checktype(L, 1, LUA_TTABLE);
	const char* AttrName = luaL_checkstring(L, 2);
	// Store as __attr_<name> on the frame table
	FString Key = FString::Printf(TEXT("__attr_%s"), UTF8_TO_TCHAR(AttrName));
	FTCHARToUTF8 UTF8Key(*Key);
	lua_pushvalue(L, 3); // value (or nil)
	lua_setfield(L, 1, UTF8Key.Get());
	return 0;
}

// frame:GetAttribute(name)
static int LF_GetAttribute(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	const char* AttrName = luaL_checkstring(L, 2);
	FString Key = FString::Printf(TEXT("__attr_%s"), UTF8_TO_TCHAR(AttrName));
	FTCHARToUTF8 UTF8Key(*Key);
	lua_getfield(L, 1, UTF8Key.Get());
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

// Helper: compute frame position from first anchor and dimensions
static void GetFrameRect(lua_State* L, float& OutLeft, float& OutBottom, float& OutRight, float& OutTop)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	OutLeft = OutBottom = OutRight = OutTop = 0.f;

	if (!Ctx || !Ctx->FrameManager) return;
	const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
	if (!Def) return;

	float W = Def->Width;
	float H = Def->Height;
	float X = 0.f, Y = 0.f;

	if (!Def->Anchors.IsEmpty())
	{
		X = Def->Anchors[0].OffsetX;
		Y = Def->Anchors[0].OffsetY;
	}

	// WoW UI coordinates: origin bottom-left, Y goes up
	// Approximate position from anchor offset
	OutLeft = X;
	OutBottom = -Y; // WoW Y is inverted vs UMG
	OutRight = X + W;
	OutTop = -Y + H;
}

// frame:GetCenter()
static int LF_GetCenter(lua_State* L)
{
	float Left, Bottom, Right, Top;
	GetFrameRect(L, Left, Bottom, Right, Top);
	lua_pushnumber(L, (Left + Right) * 0.5f);
	lua_pushnumber(L, (Bottom + Top) * 0.5f);
	return 2;
}

// frame:GetLeft/Right/Top/Bottom
static int LF_GetLeft(lua_State* L)
{
	float Left, Bottom, Right, Top;
	GetFrameRect(L, Left, Bottom, Right, Top);
	lua_pushnumber(L, Left);
	return 1;
}

static int LF_GetRight(lua_State* L)
{
	float Left, Bottom, Right, Top;
	GetFrameRect(L, Left, Bottom, Right, Top);
	lua_pushnumber(L, Right);
	return 1;
}

static int LF_GetTop(lua_State* L)
{
	float Left, Bottom, Right, Top;
	GetFrameRect(L, Left, Bottom, Right, Top);
	lua_pushnumber(L, Top);
	return 1;
}

static int LF_GetBottom(lua_State* L)
{
	float Left, Bottom, Right, Top;
	GetFrameRect(L, Left, Bottom, Right, Top);
	lua_pushnumber(L, Bottom);
	return 1;
}

// frame:SetTexture(texture) — for Button Normal/Pushed/etc textures
static int LF_SetNormalTexture(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_isnil(L, 2) || lua_isnone(L, 2)) return 0;
	const char* TexturePath = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";

	// Store in Lua table
	lua_pushstring(L, TexturePath);
	lua_setfield(L, 1, "__normalTexture");

	// Update UMG widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UButton* Button = Cast<UButton>(Widget))
		{
			UTexture2D* Texture = Ctx->FrameManager->LoadUITexture(UTF8_TO_TCHAR(TexturePath));
			if (Texture)
			{
				FSlateBrush NormalBrush;
				NormalBrush.SetResourceObject(Texture);
				NormalBrush.DrawAs = ESlateBrushDrawType::Image;
				FButtonStyle ButtonStyle = Button->GetStyle();
				ButtonStyle.Normal = NormalBrush;
				Button->SetStyle(ButtonStyle);
			}
		}
	}

	return 0;
}

static int LF_SetPushedTexture(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_isnil(L, 2) || lua_isnone(L, 2)) return 0;
	const char* TexturePath = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";

	// Store in Lua table
	lua_pushstring(L, TexturePath);
	lua_setfield(L, 1, "__pushedTexture");

	// Update UMG widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UButton* Button = Cast<UButton>(Widget))
		{
			UTexture2D* Texture = Ctx->FrameManager->LoadUITexture(UTF8_TO_TCHAR(TexturePath));
			if (Texture)
			{
				FSlateBrush PressedBrush;
				PressedBrush.SetResourceObject(Texture);
				PressedBrush.DrawAs = ESlateBrushDrawType::Image;
				FButtonStyle ButtonStyle = Button->GetStyle();
				ButtonStyle.Pressed = PressedBrush;
				Button->SetStyle(ButtonStyle);
			}
		}
	}

	return 0;
}

static int LF_SetHighlightTexture(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_isnil(L, 2) || lua_isnone(L, 2)) return 0;
	const char* TexturePath = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";

	// Store in Lua table
	lua_pushstring(L, TexturePath);
	lua_setfield(L, 1, "__highlightTexture");

	// Update UMG widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UButton* Button = Cast<UButton>(Widget))
		{
			UTexture2D* Texture = Ctx->FrameManager->LoadUITexture(UTF8_TO_TCHAR(TexturePath));
			if (Texture)
			{
				FSlateBrush HoveredBrush;
				HoveredBrush.SetResourceObject(Texture);
				HoveredBrush.DrawAs = ESlateBrushDrawType::Image;
				FButtonStyle ButtonStyle = Button->GetStyle();
				ButtonStyle.Hovered = HoveredBrush;
				Button->SetStyle(ButtonStyle);
			}
		}
	}

	return 0;
}

static int LF_SetDisabledTexture(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	if (lua_isnil(L, 2) || lua_isnone(L, 2)) return 0;
	const char* TexturePath = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";

	// Store in Lua table
	lua_pushstring(L, TexturePath);
	lua_setfield(L, 1, "__disabledTexture");

	// Update UMG widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UButton* Button = Cast<UButton>(Widget))
		{
			UTexture2D* Texture = Ctx->FrameManager->LoadUITexture(UTF8_TO_TCHAR(TexturePath));
			if (Texture)
			{
				FSlateBrush DisabledBrush;
				DisabledBrush.SetResourceObject(Texture);
				DisabledBrush.DrawAs = ESlateBrushDrawType::Image;
				FButtonStyle ButtonStyle = Button->GetStyle();
				ButtonStyle.Disabled = DisabledBrush;
				Button->SetStyle(ButtonStyle);
			}
		}
	}

	return 0;
}

// Button methods
static int LF_SetText(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	const char* Text = luaL_optstring(L, 2, ""); // WoW treats nil as empty string

	// Store in Lua table
	lua_pushstring(L, Text);
	lua_setfield(L, 1, "__text");

	// Update UMG widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UButton* Button = Cast<UButton>(Widget))
		{
			// UButton doesn't have SetText directly - this would need child text widget handling
			// For now, just store in Lua table
		}
		else if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			FText TextContent = FText::FromString(UTF8_TO_TCHAR(Text));
			TextBlock->SetText(TextContent);
		}
		else if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			FText EditContent = FText::FromString(UTF8_TO_TCHAR(Text));
			EditBox->SetText(EditContent);
		}
	}

	return 0;
}

static int LF_GetText(lua_State* L)
{
	// Try to get text from widget first
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);

		if (UButton* Button = Cast<UButton>(Widget))
		{
			// UButton doesn't have GetText directly - would need child text widget handling
			// Fall back to stored value
		}
		else if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			FString EditText = EditBox->GetText().ToString();
			lua_pushstring(L, (const char*)StringCast<UTF8CHAR>(*EditText).Get());
			return 1;
		}
		else if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			FString TextContent = TextBlock->GetText().ToString();
			lua_pushstring(L, (const char*)StringCast<UTF8CHAR>(*TextContent).Get());
			return 1;
		}
	}

	// Fallback to stored value
	lua_getfield(L, 1, "__text");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushstring(L, "");
	}
	return 1;
}

static int LF_Disable(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushboolean(L, 0);
	lua_setfield(L, 1, "__enabled");
	return 0;
}

static int LF_Enable(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushboolean(L, 1);
	lua_setfield(L, 1, "__enabled");
	return 0;
}

static int LF_IsEnabled(lua_State* L)
{
	lua_getfield(L, 1, "__enabled");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		lua_pushboolean(L, 1);
	}
	return 1;
}

static int LF_Click(lua_State* L) { return 0; }

// StatusBar methods
static int LF_SetMinMaxValues(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float MinValue = static_cast<float>(luaL_checknumber(L, 2));
	float MaxValue = static_cast<float>(luaL_checknumber(L, 3));

	// Store in Lua table
	lua_pushnumber(L, MinValue);
	lua_setfield(L, 1, "__minValue");
	lua_pushnumber(L, MaxValue);
	lua_setfield(L, 1, "__maxValue");

	// Update UMG widget if current value exists
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UProgressBar* ProgressBar = Ctx->FrameManager->GetStatusBarWidget(Handle);
		if (ProgressBar)
		{
			// Get current value and recalculate percentage
			lua_getfield(L, 1, "__value");
			float CurrentValue = lua_isnil(L, -1) ? 0.0f : static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);

			float Range = MaxValue - MinValue;
			float Percent = (Range > 0.0f) ? (CurrentValue - MinValue) / Range : 0.0f;
			ProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
		}
	}

	return 0;
}

static int LF_GetMinMaxValues(lua_State* L)
{
	lua_getfield(L, 1, "__minValue");
	if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushnumber(L, 0); }
	lua_getfield(L, 1, "__maxValue");
	if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushnumber(L, 100); }
	return 2;
}

static int LF_SetValue(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float Value = static_cast<float>(luaL_checknumber(L, 2));

	// Store in Lua table
	lua_pushnumber(L, Value);
	lua_setfield(L, 1, "__value");

	// Update UMG widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UProgressBar* ProgressBar = Ctx->FrameManager->GetStatusBarWidget(Handle);
		if (ProgressBar)
		{
			// Get min/max values from Lua table
			lua_getfield(L, 1, "__minValue");
			float MinValue = lua_isnil(L, -1) ? 0.0f : static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);

			lua_getfield(L, 1, "__maxValue");
			float MaxValue = lua_isnil(L, -1) ? 100.0f : static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);

			// Calculate percentage and update progress bar
			float Range = MaxValue - MinValue;
			float Percent = (Range > 0.0f) ? (Value - MinValue) / Range : 0.0f;
			ProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
		}
	}

	return 0;
}

static int LF_GetValue(lua_State* L)
{
	lua_getfield(L, 1, "__value");
	if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushnumber(L, 0); }
	return 1;
}

static int LF_SetStatusBarTexture(lua_State* L) { return 0; }
static int LF_SetStatusBarColor(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float R = static_cast<float>(luaL_checknumber(L, 2));
	float G = static_cast<float>(luaL_checknumber(L, 3));
	float B = static_cast<float>(luaL_checknumber(L, 4));
	float A = lua_isnumber(L, 5) ? static_cast<float>(lua_tonumber(L, 5)) : 1.0f;

	// Store in Lua table
	lua_pushnumber(L, R); lua_setfield(L, 1, "__barColorR");
	lua_pushnumber(L, G); lua_setfield(L, 1, "__barColorG");
	lua_pushnumber(L, B); lua_setfield(L, 1, "__barColorB");
	lua_pushnumber(L, A); lua_setfield(L, 1, "__barColorA");

	// Update UMG widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UProgressBar* ProgressBar = Ctx->FrameManager->GetStatusBarWidget(Handle);
		if (ProgressBar)
		{
			FLinearColor Color(R, G, B, A);
			ProgressBar->SetFillColorAndOpacity(Color);
		}
	}

	return 0;
}
static int LF_SetOrientation(lua_State* L) { return 0; }

// EditBox methods
static int LF_SetMaxLetters(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	int MaxLetters = static_cast<int>(luaL_checknumber(L, 2));

	// Store in Lua table
	lua_pushnumber(L, MaxLetters);
	lua_setfield(L, 1, "__maxLetters");

	// Apply to widget
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			// UEditableTextBox doesn't have a direct max length property in UE5
			// This would need custom implementation
		}
	}

	return 0;
}

static int LF_SetAutoFocus(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	bool AutoFocus = lua_toboolean(L, 2);

	// Store in Lua table
	lua_pushboolean(L, AutoFocus);
	lua_setfield(L, 1, "__autoFocus");

	return 0;
}

static int LF_ClearFocus(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			// Clear focus from the edit box
			EditBox->SetUserFocus(nullptr);
		}
	}

	return 0;
}

static int LF_SetFocus(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			// Set focus to the edit box
			EditBox->SetKeyboardFocus();
		}
	}

	return 0;
}

static int LF_HasFocus(lua_State* L)
{
	bool HasFocus = false;

	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		int64 Handle = GetFrameHandle(L);
		UWidget* Widget = Ctx->FrameManager->GetWidgetForHandle(Handle);
		if (UEditableTextBox* EditBox = Cast<UEditableTextBox>(Widget))
		{
			HasFocus = EditBox->HasKeyboardFocus();
		}
	}

	lua_pushboolean(L, HasFocus);
	return 1;
}
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
static int LF_SetCooldown(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	float StartTime = static_cast<float>(luaL_checknumber(L, 2));
	float Duration = static_cast<float>(luaL_checknumber(L, 3));

	// Store in Lua table
	lua_pushnumber(L, StartTime);
	lua_setfield(L, 1, "__cooldownStart");
	lua_pushnumber(L, Duration);
	lua_setfield(L, 1, "__cooldownDuration");

	// Calculate remaining time
	// TODO: Get current game time instead of using system time
	float CurrentTime = FPlatformTime::Seconds();
	float Remaining = (StartTime + Duration) - CurrentTime;

	lua_pushnumber(L, FMath::Max(0.0f, Remaining));
	lua_setfield(L, 1, "__cooldownRemaining");

	return 0;
}

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

// ── Fixed Frame API Methods ───────────────────────────────────────────────────────

// frame:GetScale() — returns the frame's explicit scale (default 1.0)
// For now, always return 1.0 since we don't track per-frame scale yet
static int LF_GetScale(lua_State* L)
{
	lua_pushnumber(L, 1.0);
	return 1;
}

// frame:SetScale(scale) — stub, would need to affect widget render transform
static int LF_SetScale(lua_State* L)
{
	// TODO: Apply render transform scale to the widget
	return 0;
}

// frame:GetEffectiveScale() — returns the cumulative scale (frame * parent * UIScale)
static int LF_GetEffectiveScale(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	if (Ctx && Ctx->FrameManager)
	{
		lua_pushnumber(L, Ctx->FrameManager->GetUIScale());
	}
	else
	{
		lua_pushnumber(L, 1.0);
	}
	return 1;
}

// frame:GetPoint(index) — returns anchor info for the nth anchor (1-based)
static int LF_GetPoint(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (!Ctx || !Ctx->FrameManager) return 0;

	int Index = static_cast<int>(luaL_optnumber(L, 2, 1)) - 1; // 1-based to 0-based
	const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
	if (!Def || Index < 0 || Index >= Def->Anchors.Num()) return 0;

	const FWowAnchor& A = Def->Anchors[Index];
	lua_pushstring(L, AnchorPointToString(A.Point));
	// Push relativeTo frame (as table or nil)
	if (!A.RelativeTo.IsEmpty())
	{
		lua_getglobal(L, TCHAR_TO_UTF8(*A.RelativeTo));
	}
	else
	{
		lua_pushnil(L);
	}
	lua_pushstring(L, AnchorPointToString(A.RelativePoint));
	lua_pushnumber(L, A.OffsetX);
	lua_pushnumber(L, A.OffsetY);
	return 5;
}

// frame:GetNumPoints() — returns the number of anchors
static int LF_GetNumPoints(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		if (Def)
		{
			lua_pushnumber(L, Def->Anchors.Num());
			return 1;
		}
	}
	lua_pushnumber(L, 0);
	return 1;
}

// frame:GetRect() — returns left, bottom, width, height in WoW coordinates
static int LF_GetRect(lua_State* L)
{
	FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
	int64 Handle = GetFrameHandle(L);
	if (Ctx && Ctx->FrameManager)
	{
		const FWowFrameDef* Def = Ctx->FrameManager->GetFrameDef(Handle);
		if (Def)
		{
			// Returns left, bottom, width, height in WoW coordinates
			// For simplicity, return based on frame rect
			lua_pushnumber(L, 0); // left
			lua_pushnumber(L, 0); // bottom
			lua_pushnumber(L, Def->Width);
			lua_pushnumber(L, Def->Height);
			return 4;
		}
	}
	return 0;
}

// button:RegisterForClicks(...) — accept click types but don't need to store them
static int LF_RegisterForClicks(lua_State* L)
{
	// Accept any number of string arguments: "LeftButtonUp", "RightButtonUp", etc.
	// We don't need to do anything with them yet — click routing is handled elsewhere
	return 0;
}

// scrollingmsgframe:AddMessage(text, r, g, b) — for now, just log the message
static int LF_AddMessage(lua_State* L)
{
	const char* Text = luaL_checkstring(L, 2);
	UE_LOG(LogWowLuaFrame, Log, TEXT("ScrollingMessageFrame:AddMessage: %s"), UTF8_TO_TCHAR(Text));
	return 0;
}

// (FontString API functions defined above near LF_CreateFontString)

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

	// Additional stubs to prevent OnUpdate error spam
	{"IsOwned", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"GetFontString", [](lua_State* L) -> int {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, 1, "__name");
		if (lua_isstring(L, -1)) {
			// Try FrameNameText first, then FrameNameFontString
			FString BaseName = UTF8_TO_TCHAR(lua_tostring(L, -1));
			lua_pop(L, 1);
			lua_getglobal(L, TCHAR_TO_UTF8(*(BaseName + TEXT("Text"))));
			if (!lua_isnil(L, -1)) return 1;
			lua_pop(L, 1);
			lua_getglobal(L, TCHAR_TO_UTF8(*(BaseName + TEXT("FontString"))));
			if (!lua_isnil(L, -1)) return 1;
			lua_pop(L, 1);
		} else { lua_pop(L, 1); }
		// Return minimal table so callers don't crash
		lua_newtable(L);
		luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
		if (!lua_isnil(L, -1)) lua_setmetatable(L, -2); else lua_pop(L, 1);
		return 1;
	}},
	{"SetChecked", [](lua_State* L) -> int { return 0; }},
	{"GetChecked", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"SetNormalFontObject", [](lua_State* L) -> int { return 0; }},
	{"SetHighlightFontObject", [](lua_State* L) -> int { return 0; }},
	{"SetDisabledFontObject", [](lua_State* L) -> int { return 0; }},
	{"LockHighlight", [](lua_State* L) -> int { return 0; }},
	{"UnlockHighlight", [](lua_State* L) -> int { return 0; }},
	{"GetNormalTexture", [](lua_State* L) -> int {
		// Try to find the named texture global: FrameNameNormalTexture
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, 1, "__name");
		const char* frameName = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
		lua_pop(L, 1);
		if (frameName)
		{
			char globalName[256];
			snprintf(globalName, sizeof(globalName), "%sNormalTexture", frameName);
			lua_getglobal(L, globalName);
			if (!lua_isnil(L, -1)) return 1;
			lua_pop(L, 1);
		}
		// Fallback: return a stub texture table with frame metatable
		lua_newtable(L);
		luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
		if (!lua_isnil(L, -1)) lua_setmetatable(L, -2); else lua_pop(L, 1);
		return 1;
	}},
	{"GetPushedTexture", [](lua_State* L) -> int {
		lua_newtable(L);
		luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
		if (!lua_isnil(L, -1)) lua_setmetatable(L, -2); else lua_pop(L, 1);
		return 1;
	}},
	{"GetHighlightTexture", [](lua_State* L) -> int {
		lua_newtable(L);
		luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
		if (!lua_isnil(L, -1)) lua_setmetatable(L, -2); else lua_pop(L, 1);
		return 1;
	}},
	{"GetDisabledTexture", [](lua_State* L) -> int {
		lua_newtable(L);
		luaL_getmetatable(L, WowLuaApi::FRAME_METATABLE);
		if (!lua_isnil(L, -1)) lua_setmetatable(L, -2); else lua_pop(L, 1);
		return 1;
	}},
	{"SetButtonState", [](lua_State* L) -> int { return 0; }},
	{"GetButtonState", [](lua_State* L) -> int { lua_pushstring(L, "NORMAL"); return 1; }},
	{"RegisterForClicks", LF_RegisterForClicks},

	// Layout/Hit testing
	{"SetHitRectInsets", [](lua_State* L) -> int { return 0; }},
	{"GetRect", LF_GetRect},
	{"GetScale", LF_GetScale},
	{"SetScale", LF_SetScale},
	{"GetEffectiveScale", LF_GetEffectiveScale},
	{"SetClipsChildren", [](lua_State* L) -> int { return 0; }},
	{"IsDragging", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"GetNumPoints", LF_GetNumPoints},
	{"GetPoint", LF_GetPoint},
	{"IsProtected", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"CanChangeProtectedState", [](lua_State* L) -> int { lua_pushboolean(L, 1); return 1; }},
	{"SetUserPlaced", [](lua_State* L) -> int { return 0; }},
	{"IsUserPlaced", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"SetDontSavePosition", [](lua_State* L) -> int { return 0; }},
	{"SetMaxResize", [](lua_State* L) -> int { return 0; }},
	{"SetMinResize", [](lua_State* L) -> int { return 0; }},

	// Texture methods on frames
	{"SetRotation", [](lua_State* L) -> int { return 0; }},
	{"SetSequence", [](lua_State* L) -> int { return 0; }},
	{"SetDesaturated", [](lua_State* L) -> int { return 0; }},
	{"IsDesaturated", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"SetBlendMode", [](lua_State* L) -> int { return 0; }},
	{"SetNonBlocking", [](lua_State* L) -> int { return 0; }},
	{"SetDrawLayer", [](lua_State* L) -> int { return 0; }},

	// Text methods on frames
	{"GetTextHeight", [](lua_State* L) -> int { lua_pushnumber(L, 12); return 1; }},
	{"SetVertexColor", [](lua_State* L) -> int { return 0; }},
	{"GetTextWidth", [](lua_State* L) -> int { lua_pushnumber(L, 0); return 1; }},
	{"GetStringWidth", [](lua_State* L) -> int { lua_pushnumber(L, 0); return 1; }},
	{"GetStringHeight", [](lua_State* L) -> int { lua_pushnumber(L, 0); return 1; }},
	{"SetTextHeight", [](lua_State* L) -> int { return 0; }},
	{"SetShadowOffset", [](lua_State* L) -> int { return 0; }},
	{"SetShadowColor", [](lua_State* L) -> int { return 0; }},
	{"SetSpacing", [](lua_State* L) -> int { return 0; }},
	{"SetJustifyH", [](lua_State* L) -> int { return 0; }},
	{"SetFormattedText", [](lua_State* L) -> int { return 0; }},
	{"SetWordWrap", [](lua_State* L) -> int { return 0; }},
	{"SetNonSpaceWrap", [](lua_State* L) -> int { return 0; }},
	{"SetJustifyV", [](lua_State* L) -> int { return 0; }},

	// Button methods
	{"SetMotionScriptsWhileDisabled", [](lua_State* L) -> int { return 0; }},

	// Missing common methods
	{"SetPadding", [](lua_State* L) -> int { return 0; }},
	{"GetPadding", [](lua_State* L) -> int { lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 4; }},
	{"SetIndentedWordWrap", [](lua_State* L) -> int { return 0; }},
	{"SetClampRectInsets", [](lua_State* L) -> int { return 0; }},
	{"GetClampRectInsets", [](lua_State* L) -> int { lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 4; }},
	{"SetClampedToScreen", [](lua_State* L) -> int { return 0; }},
	{"IsClampedToScreen", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"SetHyperlinksEnabled", [](lua_State* L) -> int { return 0; }},
	{"GetMaxLines", [](lua_State* L) -> int { lua_pushnumber(L, 0); return 1; }},
	{"SetHistoryLines", [](lua_State* L) -> int { return 0; }},
	{"SetFontObject", [](lua_State* L) -> int { return 0; }},
	{"GetFontObject", [](lua_State* L) -> int { lua_pushnil(L); return 1; }},

	// ScrollingMessageFrame methods
	{"AddMessage", LF_AddMessage},
	{"SetMaxLines", [](lua_State* L) -> int { return 0; }},
	{"SetFading", [](lua_State* L) -> int { return 0; }},
	{"SetFadeDuration", [](lua_State* L) -> int { return 0; }},
	{"SetTimeVisible", [](lua_State* L) -> int { return 0; }},
	{"SetInsertMode", [](lua_State* L) -> int { return 0; }},
	{"Clear", [](lua_State* L) -> int { return 0; }},

	// Model/Dress methods
	{"SetModel", [](lua_State* L) -> int { return 0; }},
	{"SetModelScale", [](lua_State* L) -> int { return 0; }},
	{"SetPosition", [](lua_State* L) -> int { return 0; }},
	{"SetFacing", [](lua_State* L) -> int { return 0; }},
	{"SetUnit", [](lua_State* L) -> int { return 0; }},
	{"SetCreature", [](lua_State* L) -> int { return 0; }},
	{"SetCamera", [](lua_State* L) -> int { return 0; }},
	{"RefreshUnit", [](lua_State* L) -> int { return 0; }},
	{"SetPortraitZoom", [](lua_State* L) -> int { return 0; }},
	{"ClearModel", [](lua_State* L) -> int { return 0; }},
	{"HasCustomBackground", [](lua_State* L) -> int { lua_pushboolean(L, 0); return 1; }},
	{"SetCustomRace", [](lua_State* L) -> int { return 0; }},
	{"Undress", [](lua_State* L) -> int { return 0; }},
	{"TryOn", [](lua_State* L) -> int { return 0; }},

	// Minimap methods
	{"SetZoom", [](lua_State* L) -> int { return 0; }},
	{"GetZoom", [](lua_State* L) -> int { lua_pushnumber(L, 0); return 1; }},
	{"GetZoomLevels", [](lua_State* L) -> int { lua_pushnumber(L, 5); return 1; }},
	{"SetBlipTexture", [](lua_State* L) -> int { return 0; }},
	{"SetStaticArrowTexture", [](lua_State* L) -> int { return 0; }},

	// Tooltip methods
	{"SetOwner", [](lua_State* L) -> int { return 0; }},
	{"SetUnitBuff", [](lua_State* L) -> int { return 0; }},
	{"SetUnitDebuff", [](lua_State* L) -> int { return 0; }},
	{"SetUnitAura", [](lua_State* L) -> int { return 0; }},
	{"SetInventoryItem", [](lua_State* L) -> int { return 0; }},
	{"SetBagItem", [](lua_State* L) -> int { return 0; }},
	{"SetAction", [](lua_State* L) -> int { return 0; }},
	{"SetSpell", [](lua_State* L) -> int { return 0; }},
	{"SetSpellByID", [](lua_State* L) -> int { return 0; }},
	{"SetHyperlink", [](lua_State* L) -> int { return 0; }},
	{"AddLine", [](lua_State* L) -> int { return 0; }},
	{"AddDoubleLine", [](lua_State* L) -> int { return 0; }},
	{"ClearLines", [](lua_State* L) -> int { return 0; }},
	{"NumLines", [](lua_State* L) -> int { lua_pushnumber(L, 0); return 1; }},
	{"SetMinimumWidth", [](lua_State* L) -> int { return 0; }},
	{"AppendText", [](lua_State* L) -> int { return 0; }},
	{"GetAnchorType", [](lua_State* L) -> int { lua_pushstring(L, "ANCHOR_NONE"); return 1; }},
	{"FadeOut", [](lua_State* L) -> int { return 0; }},

	// Cooldown frame
	{"SetReverse", [](lua_State* L) -> int { return 0; }},
	{"SetSwipeColor", [](lua_State* L) -> int { return 0; }},
	{"SetBlingTexture", [](lua_State* L) -> int { return 0; }},
	{"SetEdgeTexture", [](lua_State* L) -> int { return 0; }},

	// Slider
	{"SetValueStep", [](lua_State* L) -> int { return 0; }},
	{"SetObeyStepOnDrag", [](lua_State* L) -> int { return 0; }},

	// Texture access (region globals use frame metatable)
	{"SetTexture", LF_TextureSetTexture},
	{"SetTexCoord", LF_TextureSetTexCoord},
	{"GetTexture", [](lua_State* L) -> int {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, 1, "__texture");
		if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushstring(L, ""); }
		return 1;
	}},
	{"GetOwner", [](lua_State* L) -> int {
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_getfield(L, 1, "__parentHandle");
		return 1;
	}},

	// Font access
	{"SetFont", [](lua_State* L) -> int { return 0; }},
	{"GetFont", [](lua_State* L) -> int { lua_pushstring(L, "Fonts\\FRIZQT__.TTF"); lua_pushnumber(L, 12); lua_pushstring(L, ""); return 3; }},
	{"SetTextColor", [](lua_State* L) -> int { return 0; }},
	{"GetTextColor", [](lua_State* L) -> int { lua_pushnumber(L, 1); lua_pushnumber(L, 1); lua_pushnumber(L, 1); lua_pushnumber(L, 1); return 4; }},

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
