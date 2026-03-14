#include "LuaApiRegistry.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

DEFINE_LOG_CATEGORY_STATIC(LogWowLuaStub, Verbose, All);

// Macro for simple stubs that return a fixed value
#define STUB_RETURN_NIL(name) \
    static int L_##name(lua_State* L) { lua_pushnil(L); return 1; }
#define STUB_RETURN_ZERO(name) \
    static int L_##name(lua_State* L) { lua_pushnumber(L, 0); return 1; }
#define STUB_RETURN_FALSE(name) \
    static int L_##name(lua_State* L) { lua_pushboolean(L, 0); return 1; }
#define STUB_RETURN_TRUE(name) \
    static int L_##name(lua_State* L) { lua_pushboolean(L, 1); return 1; }
#define STUB_RETURN_EMPTY(name) \
    static int L_##name(lua_State* L) { lua_pushstring(L, ""); return 1; }
#define STUB_RETURN_NONE(name) \
    static int L_##name(lua_State* L) { return 0; }

// ─── Instance / Group ───────────────────────────────────────────────────────────
STUB_RETURN_FALSE(IsInInstance)
STUB_RETURN_ZERO(GetNumPartyMembers)
STUB_RETURN_ZERO(GetNumRaidMembers)
STUB_RETURN_FALSE(IsInGuild)
STUB_RETURN_ZERO(GetMoney)
STUB_RETURN_FALSE(InCombatLockdown)
STUB_RETURN_FALSE(IsResting)
STUB_RETURN_FALSE(IsMounted)
STUB_RETURN_FALSE(IsFlying)
STUB_RETURN_FALSE(IsSwimming)
STUB_RETURN_FALSE(IsFalling)
STUB_RETURN_FALSE(IsStealthed)

// GetPlayerMapPosition("player") → 0, 0
static int L_GetPlayerMapPosition(lua_State* L)
{
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 2;
}

// ─── Unit API stubs ─────────────────────────────────────────────────────────────
static int L_UnitName(lua_State* L)
{
    const char* unit = luaL_optstring(L, 1, "player");
    if (strcmp(unit, "player") == 0)
        lua_pushstring(L, "WowTestUser");
    else
        lua_pushnil(L);
    return 1;
}

static int L_UnitLevel(lua_State* L)
{
    lua_pushnumber(L, 80);
    return 1;
}

static int L_UnitHealth(lua_State* L)
{
    lua_pushnumber(L, 100);
    return 1;
}

static int L_UnitHealthMax(lua_State* L)
{
    lua_pushnumber(L, 100);
    return 1;
}

static int L_UnitPower(lua_State* L)
{
    lua_pushnumber(L, 100);
    return 1;
}

static int L_UnitPowerMax(lua_State* L)
{
    lua_pushnumber(L, 100);
    return 1;
}

static int L_UnitClass(lua_State* L)
{
    lua_pushstring(L, "Warrior");
    lua_pushstring(L, "WARRIOR");
    return 2;
}

static int L_UnitRace(lua_State* L)
{
    lua_pushstring(L, "Human");
    lua_pushstring(L, "Human");
    return 2;
}

STUB_RETURN_FALSE(UnitIsDead)
STUB_RETURN_TRUE(UnitIsPlayer)

static int L_UnitExists(lua_State* L)
{
    const char* unit = luaL_optstring(L, 1, "player");
    lua_pushboolean(L, strcmp(unit, "player") == 0 ? 1 : 0);
    return 1;
}

static int L_UnitGUID(lua_State* L)
{
    lua_pushstring(L, "0x0000000000000001");
    return 1;
}

// ─── Locale / client ────────────────────────────────────────────────────────────
static int L_GetLocale(lua_State* L)
{
    lua_pushstring(L, "enUS");
    return 1;
}

static int L_GetBuildInfo(lua_State* L)
{
    lua_pushstring(L, "3.3.5");     // version
    lua_pushnumber(L, 12340);        // build
    lua_pushstring(L, "Dec 12 2009"); // date
    lua_pushnumber(L, 30300);        // tocversion
    return 4;
}

static int L_GetRealmName(lua_State* L)
{
    lua_pushstring(L, "WowUnreal");
    return 1;
}

// ─── Screen / resolution ────────────────────────────────────────────────────────
static int L_GetScreenWidth(lua_State* L)
{
    lua_pushnumber(L, 1920);
    return 1;
}

static int L_GetScreenHeight(lua_State* L)
{
    lua_pushnumber(L, 1080);
    return 1;
}

// ─── Chat stubs ─────────────────────────────────────────────────────────────────
STUB_RETURN_NONE(SendChatMessage)
STUB_RETURN_ZERO(GetNumLanguages)
STUB_RETURN_EMPTY(GetDefaultLanguage)

// ─── Action bar stubs ───────────────────────────────────────────────────────────
STUB_RETURN_FALSE(HasAction)
STUB_RETURN_NIL(GetActionInfo)
STUB_RETURN_NIL(GetActionTexture)
STUB_RETURN_NONE(UseAction)

// ─── Spell stubs ────────────────────────────────────────────────────────────────
STUB_RETURN_NIL(GetSpellInfo)
STUB_RETURN_NIL(GetSpellCooldown)
STUB_RETURN_NONE(CastSpellByName)
STUB_RETURN_NONE(CastSpellByID)

// ─── Item stubs ─────────────────────────────────────────────────────────────────
STUB_RETURN_NIL(GetItemInfo)
STUB_RETURN_NIL(GetContainerItemInfo)

static int L_GetContainerNumSlots(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

// ─── Misc stubs ─────────────────────────────────────────────────────────────────
STUB_RETURN_NONE(TargetUnit)
STUB_RETURN_NONE(ClearTarget)
STUB_RETURN_FALSE(IsLoggedIn)
STUB_RETURN_ZERO(GetFramerate)
STUB_RETURN_NONE(SetCVar)
STUB_RETURN_NIL(GetCVar)
STUB_RETURN_FALSE(GetCVarBool)
STUB_RETURN_NONE(RegisterCVar)
STUB_RETURN_NIL(GetAddOnInfo)
STUB_RETURN_ZERO(GetNumAddOns)
STUB_RETURN_FALSE(IsAddOnLoaded)
STUB_RETURN_NONE(EnableAddOn)
STUB_RETURN_NONE(DisableAddOn)
STUB_RETURN_NONE(LoadAddOn)
STUB_RETURN_ZERO(GetNumBindings)
STUB_RETURN_NIL(GetBinding)
STUB_RETURN_NIL(GetBindingKey)
STUB_RETURN_NIL(GetBindingAction)
STUB_RETURN_FALSE(RunBinding)
STUB_RETURN_NONE(SetBinding)
STUB_RETURN_NONE(SaveBindings)

// ─── Registration ───────────────────────────────────────────────────────────────

void WowLuaApi::RegisterStubs(lua_State* L)
{
    // Instance / group
    lua_register(L, "IsInInstance", L_IsInInstance);
    lua_register(L, "GetNumPartyMembers", L_GetNumPartyMembers);
    lua_register(L, "GetNumRaidMembers", L_GetNumRaidMembers);
    lua_register(L, "IsInGuild", L_IsInGuild);
    lua_register(L, "GetMoney", L_GetMoney);
    lua_register(L, "InCombatLockdown", L_InCombatLockdown);
    lua_register(L, "IsResting", L_IsResting);
    lua_register(L, "IsMounted", L_IsMounted);
    lua_register(L, "IsFlying", L_IsFlying);
    lua_register(L, "IsSwimming", L_IsSwimming);
    lua_register(L, "IsFalling", L_IsFalling);
    lua_register(L, "IsStealthed", L_IsStealthed);
    lua_register(L, "GetPlayerMapPosition", L_GetPlayerMapPosition);

    // Unit API
    lua_register(L, "UnitName", L_UnitName);
    lua_register(L, "UnitLevel", L_UnitLevel);
    lua_register(L, "UnitHealth", L_UnitHealth);
    lua_register(L, "UnitHealthMax", L_UnitHealthMax);
    lua_register(L, "UnitPower", L_UnitPower);
    lua_register(L, "UnitPowerMax", L_UnitPowerMax);
    lua_register(L, "UnitClass", L_UnitClass);
    lua_register(L, "UnitRace", L_UnitRace);
    lua_register(L, "UnitIsDead", L_UnitIsDead);
    lua_register(L, "UnitIsPlayer", L_UnitIsPlayer);
    lua_register(L, "UnitExists", L_UnitExists);
    lua_register(L, "UnitGUID", L_UnitGUID);

    // Locale / client
    lua_register(L, "GetLocale", L_GetLocale);
    lua_register(L, "GetBuildInfo", L_GetBuildInfo);
    lua_register(L, "GetRealmName", L_GetRealmName);

    // Screen
    lua_register(L, "GetScreenWidth", L_GetScreenWidth);
    lua_register(L, "GetScreenHeight", L_GetScreenHeight);

    // Chat
    lua_register(L, "SendChatMessage", L_SendChatMessage);
    lua_register(L, "GetNumLanguages", L_GetNumLanguages);
    lua_register(L, "GetDefaultLanguage", L_GetDefaultLanguage);

    // Action bar
    lua_register(L, "HasAction", L_HasAction);
    lua_register(L, "GetActionInfo", L_GetActionInfo);
    lua_register(L, "GetActionTexture", L_GetActionTexture);
    lua_register(L, "UseAction", L_UseAction);

    // Spell
    lua_register(L, "GetSpellInfo", L_GetSpellInfo);
    lua_register(L, "GetSpellCooldown", L_GetSpellCooldown);
    lua_register(L, "CastSpellByName", L_CastSpellByName);
    lua_register(L, "CastSpellByID", L_CastSpellByID);

    // Item
    lua_register(L, "GetItemInfo", L_GetItemInfo);
    lua_register(L, "GetContainerItemInfo", L_GetContainerItemInfo);
    lua_register(L, "GetContainerNumSlots", L_GetContainerNumSlots);

    // Target
    lua_register(L, "TargetUnit", L_TargetUnit);
    lua_register(L, "ClearTarget", L_ClearTarget);

    // Misc
    lua_register(L, "IsLoggedIn", L_IsLoggedIn);
    lua_register(L, "GetFramerate", L_GetFramerate);
    lua_register(L, "SetCVar", L_SetCVar);
    lua_register(L, "GetCVar", L_GetCVar);
    lua_register(L, "GetCVarBool", L_GetCVarBool);
    lua_register(L, "RegisterCVar", L_RegisterCVar);
    lua_register(L, "GetAddOnInfo", L_GetAddOnInfo);
    lua_register(L, "GetNumAddOns", L_GetNumAddOns);
    lua_register(L, "IsAddOnLoaded", L_IsAddOnLoaded);
    lua_register(L, "EnableAddOn", L_EnableAddOn);
    lua_register(L, "DisableAddOn", L_DisableAddOn);
    lua_register(L, "LoadAddOn", L_LoadAddOn);
    lua_register(L, "GetNumBindings", L_GetNumBindings);
    lua_register(L, "GetBinding", L_GetBinding);
    lua_register(L, "GetBindingKey", L_GetBindingKey);
    lua_register(L, "GetBindingAction", L_GetBindingAction);
    lua_register(L, "RunBinding", L_RunBinding);
    lua_register(L, "SetBinding", L_SetBinding);
    lua_register(L, "SaveBindings", L_SaveBindings);

    UE_LOG(LogWowLuaStub, Log, TEXT("Registered WoW Lua stubs (~50 functions)"));
}

#else
void WowLuaApi::RegisterStubs(lua_State*) {}
#endif
