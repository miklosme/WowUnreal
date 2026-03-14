#include "LuaApiRegistry.h"
#include "WowEntityManager.h"
#include "WowEntity.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowUpdateFields.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/SpellDbc.h"

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

// Helper: resolve "player"/"target" unit strings to entity
static FWowEntity* ResolveUnit(lua_State* L, int ArgIdx = 1)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->EntityManager) return nullptr;

    const char* unit = luaL_optstring(L, ArgIdx, "player");

    if (strcmp(unit, "player") == 0)
    {
        return Ctx->EntityManager->GetLocalPlayer();
    }
    if (strcmp(unit, "target") == 0 && Ctx->ConnectionManager)
    {
        int64 TargetGuid = Ctx->ConnectionManager->GetTargetGuid();
        if (TargetGuid != 0)
        {
            return Ctx->EntityManager->Find(static_cast<uint64>(TargetGuid));
        }
    }
    return nullptr;
}

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

// ─── Unit API ─────────────────────────────────────────────────────────────────
static int L_UnitName(lua_State* L)
{
    // TODO: name lookup requires NameCache (SMSG_NAME_QUERY_RESPONSE)
    const char* unit = luaL_optstring(L, 1, "player");
    if (strcmp(unit, "player") == 0)
        lua_pushstring(L, "Player");
    else
        lua_pushnil(L);
    return 1;
}

static int L_UnitLevel(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushnumber(L, Entity->GetLevel());
    else
        lua_pushnumber(L, 0);
    return 1;
}

static int L_UnitHealth(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushnumber(L, Entity->GetHealth());
    else
        lua_pushnumber(L, 0);
    return 1;
}

static int L_UnitHealthMax(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushnumber(L, Entity->GetMaxHealth());
    else
        lua_pushnumber(L, 0);
    return 1;
}

static int L_UnitPower(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushnumber(L, static_cast<int32>(Entity->GetField(UnitField::POWER1)));
    else
        lua_pushnumber(L, 0);
    return 1;
}

static int L_UnitPowerMax(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushnumber(L, static_cast<int32>(Entity->GetField(UnitField::MAXPOWER1)));
    else
        lua_pushnumber(L, 0);
    return 1;
}

static int L_UnitClass(lua_State* L)
{
    // TODO: resolve from entity class field + ChrClasses.dbc
    lua_pushstring(L, "Warrior");
    lua_pushstring(L, "WARRIOR");
    return 2;
}

static int L_UnitRace(lua_State* L)
{
    // TODO: resolve from entity race field + ChrRaces.dbc
    lua_pushstring(L, "Human");
    lua_pushstring(L, "Human");
    return 2;
}

static int L_UnitIsDead(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushboolean(L, Entity->GetHealth() <= 0 ? 1 : 0);
    else
        lua_pushboolean(L, 0);
    return 1;
}

static int L_UnitIsPlayer(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushboolean(L, Entity->IsPlayer() ? 1 : 0);
    else
        lua_pushboolean(L, 0);
    return 1;
}

static int L_UnitExists(lua_State* L)
{
    const char* unit = luaL_optstring(L, 1, "player");
    if (strcmp(unit, "player") == 0)
    {
        lua_pushboolean(L, 1);
    }
    else
    {
        FWowEntity* Entity = ResolveUnit(L);
        lua_pushboolean(L, Entity ? 1 : 0);
    }
    return 1;
}

static int L_UnitGUID(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%016llX", static_cast<unsigned long long>(Entity->Guid));
        lua_pushstring(L, buf);
    }
    else
    {
        lua_pushstring(L, "0x0000000000000000");
    }
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
    // TODO: read from GEngine->GameViewport
    lua_pushnumber(L, 1920);
    return 1;
}

static int L_GetScreenHeight(lua_State* L)
{
    // TODO: read from GEngine->GameViewport
    lua_pushnumber(L, 1080);
    return 1;
}

// ─── Chat ─────────────────────────────────────────────────────────────────────
static int L_SendChatMessage(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const char* msg = luaL_optstring(L, 1, "");
        const char* chatType = luaL_optstring(L, 2, "SAY");
        int32 Type = 1; // SAY
        if (strcmp(chatType, "YELL") == 0) Type = 6;
        else if (strcmp(chatType, "PARTY") == 0) Type = 2;
        else if (strcmp(chatType, "GUILD") == 0) Type = 4;
        Ctx->ConnectionManager->SendChatMessage(FString(msg), Type);
    }
    return 0;
}

STUB_RETURN_ZERO(GetNumLanguages)
STUB_RETURN_EMPTY(GetDefaultLanguage)

// ─── Action bar stubs ───────────────────────────────────────────────────────────
STUB_RETURN_FALSE(HasAction)
STUB_RETURN_NIL(GetActionInfo)
STUB_RETURN_NIL(GetActionTexture)
STUB_RETURN_NONE(UseAction)

// ─── Spell functions ────────────────────────────────────────────────────────────
STUB_RETURN_NIL(GetSpellInfo)
STUB_RETURN_NIL(GetSpellCooldown)

static int L_CastSpellByID(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->ConnectionManager) return 0;

    int32 SpellId = static_cast<int32>(luaL_checknumber(L, 1));
    int64 TargetGuid = Ctx->ConnectionManager->GetTargetGuid();
    Ctx->ConnectionManager->SendCastSpell(SpellId, TargetGuid);
    return 0;
}

static int L_CastSpellByName(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->ConnectionManager) return 0;

    const char* SpellName = luaL_checkstring(L, 1);
    FString NameStr = UTF8_TO_TCHAR(SpellName);

    // Strip rank suffix like "Fireball(Rank 1)" — WoW API convention
    int32 ParenIdx = INDEX_NONE;
    if (NameStr.FindChar(TEXT('('), ParenIdx))
    {
        NameStr.LeftInline(ParenIdx);
        NameStr.TrimEndInline();
    }

    // Look up spell ID from DBC by name, matching against known spells
    const FSpellDbc& Spells = FDbcStore::Get().Spells();
    const FWowPacketHandler& Handler = Ctx->ConnectionManager->PacketHandler;
    uint32 FoundSpellId = 0;

    for (const FSpellDbcEntry& Entry : Spells.GetAll())
    {
        if (Entry.SpellName.Equals(NameStr, ESearchCase::IgnoreCase) && Handler.KnownSpells.Contains(Entry.ID))
        {
            // Pick the highest-rank known spell with this name
            FoundSpellId = Entry.ID;
        }
    }

    if (FoundSpellId != 0)
    {
        int64 TargetGuid = Ctx->ConnectionManager->GetTargetGuid();
        Ctx->ConnectionManager->SendCastSpell(FoundSpellId, TargetGuid);
    }

    return 0;
}

// ─── Item stubs ─────────────────────────────────────────────────────────────────
STUB_RETURN_NIL(GetItemInfo)
STUB_RETURN_NIL(GetContainerItemInfo)

static int L_GetContainerNumSlots(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

// ─── Target ─────────────────────────────────────────────────────────────────────
static int L_TargetUnit(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager && Ctx->EntityManager)
    {
        const char* unit = luaL_optstring(L, 1, "");
        // TODO: resolve unit name to GUID
    }
    return 0;
}

static int L_ClearTarget(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        Ctx->ConnectionManager->SendSetSelection(0);
    }
    return 0;
}

// ─── Combat ─────────────────────────────────────────────────────────────────────
static int L_AttackTarget(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        int64 TargetGuid = Ctx->ConnectionManager->GetTargetGuid();
        if (TargetGuid != 0)
        {
            Ctx->ConnectionManager->SendAttackSwing(TargetGuid);
        }
    }
    return 0;
}

static int L_StopAttack(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        Ctx->ConnectionManager->SendAttackStop();
    }
    return 0;
}

// ─── Misc ─────────────────────────────────────────────────────────────────────
static int L_IsLoggedIn(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        lua_pushboolean(L, Ctx->ConnectionManager->GetState() == EWowSessionState::WorldInGame ? 1 : 0);
    }
    else
    {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int L_GetFramerate(lua_State* L)
{
    float DeltaTime = FApp::GetDeltaTime();
    lua_pushnumber(L, DeltaTime > 0.0f ? 1.0f / DeltaTime : 0.0f);
    return 1;
}

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

    // Combat
    lua_register(L, "AttackTarget", L_AttackTarget);
    lua_register(L, "StopAttack", L_StopAttack);

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

    UE_LOG(LogWowLuaStub, Log, TEXT("Registered WoW Lua API (~50 functions, entity-backed unit API)"));
}

#else
void WowLuaApi::RegisterStubs(lua_State*) {}
#endif
