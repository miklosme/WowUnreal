#include "LuaApiRegistry.h"
#include "WowEntityManager.h"
#include "WowEntity.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowUpdateFields.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/SpellDbc.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/App.h"

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
static int L_GetMoney(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            lua_pushnumber(L, Player->GetCoinage());
            return 1;
        }
    }
    lua_pushnumber(L, 0);
    return 1;
}
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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    const char* unit = luaL_optstring(L, 1, "player");

    if (strcmp(unit, "player") == 0 && Ctx && Ctx->ConnectionManager)
    {
        FString CharName = Ctx->ConnectionManager->GetCharacterName();
        if (!CharName.IsEmpty())
        {
            lua_pushstring(L, TCHAR_TO_UTF8(*CharName));
            return 1;
        }
    }

    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
    {
        // For non-player entities, use display name if available
        lua_pushstring(L, "Unknown");
    }
    else
    {
        lua_pushnil(L);
    }
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

// WoW class ID → (display name, token) — 3.3.5a class IDs
static const char* WowClassNames[] = {
    "Unknown", "Warrior", "Paladin", "Hunter", "Rogue",
    "Priest", "Death Knight", "Shaman", "Mage", "Warlock",
    "Unknown", "Druid"
};
static const char* WowClassTokens[] = {
    "UNKNOWN", "WARRIOR", "PALADIN", "HUNTER", "ROGUE",
    "PRIEST", "DEATHKNIGHT", "SHAMAN", "MAGE", "WARLOCK",
    "UNKNOWN", "DRUID"
};

static int L_UnitClass(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        if (Entity->IsUnit())
        {
            uint8 ClassId = Unit->GetClassId();
            if (ClassId < UE_ARRAY_COUNT(WowClassNames))
            {
                lua_pushstring(L, WowClassNames[ClassId]);
                lua_pushstring(L, WowClassTokens[ClassId]);
                return 2;
            }
        }
    }
    lua_pushstring(L, "Warrior");
    lua_pushstring(L, "WARRIOR");
    return 2;
}

static int L_UnitRace(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity && Entity->IsUnit())
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        uint8 RaceId = Unit->GetRaceId();

        // Look up race name from ChrRaces.dbc
        const FChrRacesDbcEntry* RaceEntry = FDbcStore::Get().ChrRaces().GetById(RaceId);
        if (RaceEntry && !RaceEntry->Name.IsEmpty())
        {
            lua_pushstring(L, TCHAR_TO_UTF8(*RaceEntry->Name));
            lua_pushstring(L, TCHAR_TO_UTF8(*RaceEntry->Name));
            return 2;
        }
    }
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

// Additional Unit API functions
static int L_UnitIsDeadOrGhost(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity)
        lua_pushboolean(L, Entity->GetHealth() <= 0 ? 1 : 0);
    else
        lua_pushboolean(L, 0);
    return 1;
}

static int L_UnitIsFriend(lua_State* L)
{
    // For now, assume player is friendly to self, enemies to others
    const char* unit = luaL_optstring(L, 1, "player");
    lua_pushboolean(L, strcmp(unit, "player") == 0 ? 1 : 0);
    return 1;
}

static int L_UnitIsEnemy(lua_State* L)
{
    const char* unit = luaL_optstring(L, 1, "player");
    lua_pushboolean(L, strcmp(unit, "player") != 0 ? 1 : 0);
    return 1;
}

static int L_UnitAffectingCombat(lua_State* L)
{
    // TODO: Check combat status from unit flags
    lua_pushboolean(L, 0);
    return 1;
}

static int L_UnitBuff(lua_State* L)
{
    // UnitBuff(unit, index) → name, rank, icon, count, debuffType, duration, expirationTime, isFromPlayer, isStealable
    lua_pushnil(L); // name
    lua_pushnil(L); // rank
    lua_pushnil(L); // icon
    lua_pushnumber(L, 0); // count
    lua_pushnil(L); // debuffType
    lua_pushnumber(L, 0); // duration
    lua_pushnumber(L, 0); // expirationTime
    lua_pushboolean(L, 0); // isFromPlayer
    lua_pushboolean(L, 0); // isStealable
    return 9;
}

static int L_UnitDebuff(lua_State* L)
{
    // Same return structure as UnitBuff
    lua_pushnil(L); // name
    lua_pushnil(L); // rank
    lua_pushnil(L); // icon
    lua_pushnumber(L, 0); // count
    lua_pushnil(L); // debuffType
    lua_pushnumber(L, 0); // duration
    lua_pushnumber(L, 0); // expirationTime
    lua_pushboolean(L, 0); // isFromPlayer
    lua_pushboolean(L, 0); // isStealable
    return 9;
}

static int L_UnitAura(lua_State* L)
{
    // UnitAura(unit, index, filter) → name, rank, icon, count, debuffType, duration, expirationTime, isFromPlayer, isStealable
    lua_pushnil(L); // name
    lua_pushnil(L); // rank
    lua_pushnil(L); // icon
    lua_pushnumber(L, 0); // count
    lua_pushnil(L); // debuffType
    lua_pushnumber(L, 0); // duration
    lua_pushnumber(L, 0); // expirationTime
    lua_pushboolean(L, 0); // isFromPlayer
    lua_pushboolean(L, 0); // isStealable
    return 9;
}

static int L_GetUnitName(lua_State* L)
{
    // Alias for UnitName
    return L_UnitName(L);
}

static int L_UnitPowerType(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity && Entity->IsUnit())
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        lua_pushnumber(L, Unit->GetPowerTypeId());
        lua_pushstring(L, "MANA"); // Default to MANA, could map from power type
        return 2;
    }
    lua_pushnumber(L, 0);
    lua_pushstring(L, "MANA");
    return 2;
}

static int L_UnitStat(lua_State* L)
{
    // UnitStat(unit, statIndex) → base, stat, posBuff, negBuff
    lua_pushnumber(L, 10); // base
    lua_pushnumber(L, 10); // stat
    lua_pushnumber(L, 0); // posBuff
    lua_pushnumber(L, 0); // negBuff
    return 4;
}

static int L_UnitAttackPower(lua_State* L)
{
    // UnitAttackPower(unit) → base, posBuff, negBuff
    lua_pushnumber(L, 100); // base
    lua_pushnumber(L, 0); // posBuff
    lua_pushnumber(L, 0); // negBuff
    return 3;
}

static int L_UnitRangedAttackPower(lua_State* L)
{
    // UnitRangedAttackPower(unit) → base, posBuff, negBuff
    lua_pushnumber(L, 50); // base
    lua_pushnumber(L, 0); // posBuff
    lua_pushnumber(L, 0); // negBuff
    return 3;
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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        // TODO: Get realm name from connection manager
        lua_pushstring(L, "WowUnreal");
    }
    else
    {
        lua_pushstring(L, "WowUnreal");
    }
    return 1;
}

static int L_GetServerTime(lua_State* L)
{
    // Return current time as Unix timestamp
    lua_pushnumber(L, FPlatformTime::Seconds());
    return 1;
}

static int L_GetGameTime(lua_State* L)
{
    // Return hours, minutes for in-game time
    // For now, just use real time
    time_t RawTime;
    time(&RawTime);
    struct tm* TimeInfo = localtime(&RawTime);
    lua_pushnumber(L, TimeInfo->tm_hour);
    lua_pushnumber(L, TimeInfo->tm_min);
    return 2;
}

// ─── Screen / resolution ────────────────────────────────────────────────────────
static int L_GetScreenWidth(lua_State* L)
{
    FVector2D Size(1920, 1080);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(Size);
    }
    lua_pushnumber(L, Size.X);
    return 1;
}

static int L_GetScreenHeight(lua_State* L)
{
    FVector2D Size(1920, 1080);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(Size);
    }
    lua_pushnumber(L, Size.Y);
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

static int L_GetNumLanguages(lua_State* L)
{
    lua_pushnumber(L, 1); // Just Common for now
    return 1;
}

static int L_GetDefaultLanguage(lua_State* L)
{
    lua_pushstring(L, "Common");
    return 1;
}

static int L_GetLanguageByIndex(lua_State* L)
{
    // GetLanguageByIndex(index) → languageName, languageId
    lua_pushstring(L, "Common");
    lua_pushnumber(L, 7); // LANG_COMMON = 7 in WoW
    return 2;
}

// ─── Action bar stubs ───────────────────────────────────────────────────────────
STUB_RETURN_FALSE(HasAction)
STUB_RETURN_NIL(GetActionInfo)
STUB_RETURN_NIL(GetActionTexture)
STUB_RETURN_NONE(UseAction)

static int L_GetActionCount(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetActionCooldown(lua_State* L)
{
    // GetActionCooldown(slot) → start, duration, enabled
    lua_pushnumber(L, 0); // start
    lua_pushnumber(L, 0); // duration
    lua_pushnumber(L, 1); // enabled
    return 3;
}

static int L_IsCurrentAction(lua_State* L)
{
    lua_pushboolean(L, 0);
    return 1;
}

static int L_IsUsableAction(lua_State* L)
{
    // IsUsableAction(slot) → usable, noMana
    lua_pushboolean(L, 1);
    lua_pushboolean(L, 0);
    return 2;
}

static int L_IsAttackAction(lua_State* L)
{
    lua_pushboolean(L, 0);
    return 1;
}

static int L_GetBonusBarOffset(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

// Global variable for action bar page
static int L_GetCurrentActionBarPage(lua_State* L)
{
    lua_pushnumber(L, 1);
    return 1;
}

// ─── Spell functions ────────────────────────────────────────────────────────────
static int L_GetSpellInfo(lua_State* L)
{
    // GetSpellInfo(spellId) → name, rank, icon, castTime, minRange, maxRange
    int32 SpellId = static_cast<int32>(luaL_checknumber(L, 1));
    const FSpellDbcEntry* Entry = FDbcStore::Get().Spells().GetById(SpellId);
    if (Entry)
    {
        lua_pushstring(L, TCHAR_TO_UTF8(*Entry->SpellName));  // name
        lua_pushstring(L, TCHAR_TO_UTF8(*Entry->Rank));        // rank
        lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark"); // icon (placeholder)
        lua_pushnumber(L, 0);  // castTime (would need CastingTime lookup)
        lua_pushnumber(L, 0);  // minRange
        lua_pushnumber(L, 0);  // maxRange
        return 6;
    }
    lua_pushnil(L);
    return 1;
}

static int L_GetSpellCooldown(lua_State* L)
{
    // GetSpellCooldown(spellId) → start, duration, enabled
    lua_pushnumber(L, 0); // start
    lua_pushnumber(L, 0); // duration
    lua_pushnumber(L, 1); // enabled
    return 3;
}

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

static int L_GetSpellTexture(lua_State* L)
{
    // GetSpellTexture(spellId) → texture
    lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark");
    return 1;
}

static int L_GetNumSpellTabs(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetSpellTabInfo(lua_State* L)
{
    // GetSpellTabInfo(index) → name, texture, offset, numSpells
    lua_pushstring(L, "");
    lua_pushstring(L, "");
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 4;
}

static int L_IsUsableSpell(lua_State* L)
{
    // IsUsableSpell(spell) → usable, noMana
    lua_pushboolean(L, 1);
    lua_pushboolean(L, 0);
    return 2;
}

static int L_IsSpellInRange(lua_State* L)
{
    // IsSpellInRange(spell, unit) → inRange
    lua_pushboolean(L, 1);
    return 1;
}

static int L_GetSpellBookItemInfo(lua_State* L)
{
    // GetSpellBookItemInfo(index, bookType) → spellType, spellId
    lua_pushstring(L, "SPELL");
    lua_pushnumber(L, 0);
    return 2;
}

// ─── Item stubs ─────────────────────────────────────────────────────────────────
static int L_GetItemInfo(lua_State* L)
{
    // GetItemInfo(itemId) → name, link, quality, iLevel, reqLevel, class, subclass, maxStack, equipSlot, texture, vendorPrice
    lua_pushstring(L, "Unknown Item"); // name
    lua_pushnil(L); // link
    lua_pushnumber(L, 1); // quality
    lua_pushnumber(L, 1); // iLevel
    lua_pushnumber(L, 1); // reqLevel
    lua_pushstring(L, "Miscellaneous"); // class
    lua_pushstring(L, ""); // subclass
    lua_pushnumber(L, 1); // maxStack
    lua_pushstring(L, ""); // equipSlot
    lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark"); // texture
    lua_pushnumber(L, 0); // vendorPrice
    return 11;
}

static int L_GetContainerItemInfo(lua_State* L)
{
    // GetContainerItemInfo(bag, slot) → texture, count, locked, quality, readable, lootable, link
    lua_pushnil(L); // texture
    lua_pushnumber(L, 0); // count
    lua_pushboolean(L, 0); // locked
    lua_pushnumber(L, 1); // quality
    lua_pushboolean(L, 0); // readable
    lua_pushboolean(L, 0); // lootable
    lua_pushnil(L); // link
    return 7;
}

static int L_GetContainerNumSlots(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetContainerItemLink(lua_State* L)
{
    lua_pushnil(L);
    return 1;
}

static int L_GetContainerFreeSlots(lua_State* L)
{
    // GetContainerFreeSlots(bag) → freeSlots, bagFamily
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 2;
}

static int L_GetInventoryItemLink(lua_State* L)
{
    lua_pushnil(L);
    return 1;
}

static int L_GetInventoryItemTexture(lua_State* L)
{
    lua_pushnil(L);
    return 1;
}

static int L_GetInventorySlotInfo(lua_State* L)
{
    // GetInventorySlotInfo(slotName) → slotId, textureName, checkRelic
    lua_pushnumber(L, 0);
    lua_pushnil(L);
    lua_pushboolean(L, 0);
    return 3;
}

static int L_GetItemCount(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetItemIcon(lua_State* L)
{
    lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark");
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

// ─── Social ─────────────────────────────────────────────────────────────────────
static int L_GetNumFriends(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetFriendInfo(lua_State* L)
{
    // GetFriendInfo(index) → name, level, class, area, online, status, note
    lua_pushstring(L, ""); // name
    lua_pushnumber(L, 0); // level
    lua_pushstring(L, ""); // class
    lua_pushstring(L, ""); // area
    lua_pushboolean(L, 0); // online
    lua_pushstring(L, ""); // status
    lua_pushstring(L, ""); // note
    return 7;
}

static int L_GetNumGuildMembers(lua_State* L)
{
    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetGuildRosterInfo(lua_State* L)
{
    // GetGuildRosterInfo(index) → name, rank, rankIndex, level, class, zone, note, officernote, online, status, classFileName
    lua_pushstring(L, ""); // name
    lua_pushstring(L, ""); // rank
    lua_pushnumber(L, 0); // rankIndex
    lua_pushnumber(L, 0); // level
    lua_pushstring(L, ""); // class
    lua_pushstring(L, ""); // zone
    lua_pushstring(L, ""); // note
    lua_pushstring(L, ""); // officernote
    lua_pushboolean(L, 0); // online
    lua_pushstring(L, ""); // status
    lua_pushstring(L, ""); // classFileName
    return 11;
}

static int L_GetGuildInfo(lua_State* L)
{
    // GetGuildInfo(unit) → guildName, guildRankName, guildRankIndex
    lua_pushnil(L); // guildName
    lua_pushnil(L); // guildRankName
    lua_pushnumber(L, 0); // guildRankIndex
    return 3;
}

// ─── Quest ──────────────────────────────────────────────────────────────────────
static int L_GetNumQuestLogEntries(lua_State* L)
{
    lua_pushnumber(L, 0); // numEntries
    lua_pushnumber(L, 25); // numQuests (max quest log size)
    return 2;
}

static int L_GetQuestLogTitle(lua_State* L)
{
    // GetQuestLogTitle(questIndex) → questTitle, level, questTag, suggestedGroup, isHeader, isCollapsed, isComplete
    lua_pushstring(L, ""); // questTitle
    lua_pushnumber(L, 0); // level
    lua_pushstring(L, ""); // questTag
    lua_pushnumber(L, 0); // suggestedGroup
    lua_pushboolean(L, 0); // isHeader
    lua_pushboolean(L, 0); // isCollapsed
    lua_pushboolean(L, 0); // isComplete
    return 7;
}

static int L_GetQuestLogQuestText(lua_State* L)
{
    lua_pushstring(L, ""); // questText
    return 1;
}

static int L_SelectQuestLogEntry(lua_State* L)
{
    return 0;
}

static int L_IsQuestComplete(lua_State* L)
{
    lua_pushboolean(L, 0);
    return 1;
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

// ─── CVar system ────────────────────────────────────────────────────────────────
static TMap<FString, FString> CVarStorage;

static int L_SetCVar(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    CVarStorage.Add(UTF8_TO_TCHAR(name), UTF8_TO_TCHAR(value));
    return 0;
}

static int L_GetCVar(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    FString Key = UTF8_TO_TCHAR(name);
    if (FString* Value = CVarStorage.Find(Key))
    {
        FTCHARToUTF8 Conv(**Value);
        lua_pushstring(L, Conv.Get());
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

static int L_GetCVarBool(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    FString Key = UTF8_TO_TCHAR(name);
    if (FString* Value = CVarStorage.Find(Key))
    {
        lua_pushboolean(L, (*Value).Equals(TEXT("1")) || Value->ToBool() ? 1 : 0);
    }
    else
    {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int L_RegisterForSaveVariables(lua_State* L)
{
    // For addon saved variables
    return 0;
}

// ─── Sound and misc functions ───────────────────────────────────────────────────
static int L_PlaySound(lua_State* L)
{
    // const char* soundFile = luaL_checkstring(L, 1);
    // TODO: Play sound through UE5 audio system
    return 0;
}

static int L_PlaySoundFile(lua_State* L)
{
    // const char* filePath = luaL_checkstring(L, 1);
    // TODO: Play sound file through UE5 audio system
    return 0;
}

static int L_StopMusic(lua_State* L)
{
    // TODO: Stop music through UE5 audio system
    return 0;
}

static int L_SetPortraitTexture(lua_State* L)
{
    // SetPortraitTexture(texture, unit)
    return 0;
}

static int L_SetPortraitToTexture(lua_State* L)
{
    // SetPortraitToTexture(texture, texturePath)
    return 0;
}

static int L_GetMoneyString(lua_State* L)
{
    int32 copper = static_cast<int32>(luaL_checknumber(L, 1));
    int32 gold = copper / 10000;
    int32 silver = (copper % 10000) / 100;
    copper = copper % 100;

    FString Result;
    if (gold > 0)
        Result += FString::Printf(TEXT("%dg"), gold);
    if (silver > 0)
        Result += FString::Printf(TEXT("%ds"), silver);
    if (copper > 0)
        Result += FString::Printf(TEXT("%dc"), copper);
    if (Result.IsEmpty())
        Result = TEXT("0c");

    FTCHARToUTF8 Conv(*Result);
    lua_pushstring(L, Conv.Get());
    return 1;
}

static int L_GetCoinTextureString(lua_State* L)
{
    int32 amount = static_cast<int32>(luaL_checknumber(L, 1));
    const char* coinType = luaL_optstring(L, 2, "gold");

    FString texture;
    if (strcmp(coinType, "gold") == 0)
        texture = TEXT("Interface\\MoneyFrame\\UI-GoldIcon");
    else if (strcmp(coinType, "silver") == 0)
        texture = TEXT("Interface\\MoneyFrame\\UI-SilverIcon");
    else
        texture = TEXT("Interface\\MoneyFrame\\UI-CopperIcon");

    FString Result = FString::Printf(TEXT("|T%s:0|t%d"), *texture, amount);
    FTCHARToUTF8 Conv(*Result);
    lua_pushstring(L, Conv.Get());
    return 1;
}

static int L_BreakUpLargeNumbers(lua_State* L)
{
    int32 number = static_cast<int32>(luaL_checknumber(L, 1));
    FString Result = FString::Printf(TEXT("%d"), number);

    // Add commas for thousands separators
    int32 Len = Result.Len();
    if (Len > 3)
    {
        FString Formatted;
        for (int32 i = 0; i < Len; i++)
        {
            if (i > 0 && (Len - i) % 3 == 0)
                Formatted += TEXT(",");
            Formatted += Result[i];
        }
        Result = Formatted;
    }

    FTCHARToUTF8 Conv(*Result);
    lua_pushstring(L, Conv.Get());
    return 1;
}
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
    lua_register(L, "GetUnitName", L_GetUnitName);
    lua_register(L, "UnitLevel", L_UnitLevel);
    lua_register(L, "UnitHealth", L_UnitHealth);
    lua_register(L, "UnitHealthMax", L_UnitHealthMax);
    lua_register(L, "UnitPower", L_UnitPower);
    lua_register(L, "UnitPowerMax", L_UnitPowerMax);
    lua_register(L, "UnitPowerType", L_UnitPowerType);
    lua_register(L, "UnitClass", L_UnitClass);
    lua_register(L, "UnitRace", L_UnitRace);
    lua_register(L, "UnitIsDead", L_UnitIsDead);
    lua_register(L, "UnitIsDeadOrGhost", L_UnitIsDeadOrGhost);
    lua_register(L, "UnitIsPlayer", L_UnitIsPlayer);
    lua_register(L, "UnitIsFriend", L_UnitIsFriend);
    lua_register(L, "UnitIsEnemy", L_UnitIsEnemy);
    lua_register(L, "UnitAffectingCombat", L_UnitAffectingCombat);
    lua_register(L, "UnitBuff", L_UnitBuff);
    lua_register(L, "UnitDebuff", L_UnitDebuff);
    lua_register(L, "UnitAura", L_UnitAura);
    lua_register(L, "UnitExists", L_UnitExists);
    lua_register(L, "UnitGUID", L_UnitGUID);
    lua_register(L, "UnitStat", L_UnitStat);
    lua_register(L, "UnitAttackPower", L_UnitAttackPower);
    lua_register(L, "UnitRangedAttackPower", L_UnitRangedAttackPower);

    // Locale / client
    lua_register(L, "GetLocale", L_GetLocale);
    lua_register(L, "GetBuildInfo", L_GetBuildInfo);
    lua_register(L, "GetRealmName", L_GetRealmName);
    lua_register(L, "GetServerTime", L_GetServerTime);
    lua_register(L, "GetGameTime", L_GetGameTime);

    // Screen
    lua_register(L, "GetScreenWidth", L_GetScreenWidth);
    lua_register(L, "GetScreenHeight", L_GetScreenHeight);

    // Chat
    lua_register(L, "SendChatMessage", L_SendChatMessage);
    lua_register(L, "GetNumLanguages", L_GetNumLanguages);
    lua_register(L, "GetDefaultLanguage", L_GetDefaultLanguage);
    lua_register(L, "GetLanguageByIndex", L_GetLanguageByIndex);

    // Action bar
    lua_register(L, "HasAction", L_HasAction);
    lua_register(L, "GetActionInfo", L_GetActionInfo);
    lua_register(L, "GetActionTexture", L_GetActionTexture);
    lua_register(L, "GetActionCount", L_GetActionCount);
    lua_register(L, "GetActionCooldown", L_GetActionCooldown);
    lua_register(L, "IsCurrentAction", L_IsCurrentAction);
    lua_register(L, "IsUsableAction", L_IsUsableAction);
    lua_register(L, "IsAttackAction", L_IsAttackAction);
    lua_register(L, "GetBonusBarOffset", L_GetBonusBarOffset);
    lua_register(L, "UseAction", L_UseAction);

    // Spell
    lua_register(L, "GetSpellInfo", L_GetSpellInfo);
    lua_register(L, "GetSpellCooldown", L_GetSpellCooldown);
    lua_register(L, "GetSpellTexture", L_GetSpellTexture);
    lua_register(L, "GetNumSpellTabs", L_GetNumSpellTabs);
    lua_register(L, "GetSpellTabInfo", L_GetSpellTabInfo);
    lua_register(L, "IsUsableSpell", L_IsUsableSpell);
    lua_register(L, "IsSpellInRange", L_IsSpellInRange);
    lua_register(L, "GetSpellBookItemInfo", L_GetSpellBookItemInfo);
    lua_register(L, "CastSpellByName", L_CastSpellByName);
    lua_register(L, "CastSpellByID", L_CastSpellByID);

    // Item/Inventory
    lua_register(L, "GetItemInfo", L_GetItemInfo);
    lua_register(L, "GetItemCount", L_GetItemCount);
    lua_register(L, "GetItemIcon", L_GetItemIcon);
    lua_register(L, "GetContainerItemInfo", L_GetContainerItemInfo);
    lua_register(L, "GetContainerItemLink", L_GetContainerItemLink);
    lua_register(L, "GetContainerNumSlots", L_GetContainerNumSlots);
    lua_register(L, "GetContainerFreeSlots", L_GetContainerFreeSlots);
    lua_register(L, "GetInventoryItemLink", L_GetInventoryItemLink);
    lua_register(L, "GetInventoryItemTexture", L_GetInventoryItemTexture);
    lua_register(L, "GetInventorySlotInfo", L_GetInventorySlotInfo);

    // Target
    lua_register(L, "TargetUnit", L_TargetUnit);
    lua_register(L, "ClearTarget", L_ClearTarget);

    // Combat
    lua_register(L, "AttackTarget", L_AttackTarget);
    lua_register(L, "StopAttack", L_StopAttack);

    // Social
    lua_register(L, "GetNumFriends", L_GetNumFriends);
    lua_register(L, "GetFriendInfo", L_GetFriendInfo);
    lua_register(L, "GetNumGuildMembers", L_GetNumGuildMembers);
    lua_register(L, "GetGuildRosterInfo", L_GetGuildRosterInfo);
    lua_register(L, "GetGuildInfo", L_GetGuildInfo);

    // Quest
    lua_register(L, "GetNumQuestLogEntries", L_GetNumQuestLogEntries);
    lua_register(L, "GetQuestLogTitle", L_GetQuestLogTitle);
    lua_register(L, "GetQuestLogQuestText", L_GetQuestLogQuestText);
    lua_register(L, "SelectQuestLogEntry", L_SelectQuestLogEntry);
    lua_register(L, "IsQuestComplete", L_IsQuestComplete);

    // CVar
    lua_register(L, "SetCVar", L_SetCVar);
    lua_register(L, "GetCVar", L_GetCVar);
    lua_register(L, "GetCVarBool", L_GetCVarBool);
    lua_register(L, "RegisterForSaveVariables", L_RegisterForSaveVariables);

    // Sound/UI
    lua_register(L, "PlaySound", L_PlaySound);
    lua_register(L, "PlaySoundFile", L_PlaySoundFile);
    lua_register(L, "StopMusic", L_StopMusic);
    lua_register(L, "SetPortraitTexture", L_SetPortraitTexture);
    lua_register(L, "SetPortraitToTexture", L_SetPortraitToTexture);
    lua_register(L, "GetMoneyString", L_GetMoneyString);
    lua_register(L, "GetCoinTextureString", L_GetCoinTextureString);
    lua_register(L, "BreakUpLargeNumbers", L_BreakUpLargeNumbers);

    // Misc
    lua_register(L, "IsLoggedIn", L_IsLoggedIn);
    lua_register(L, "GetFramerate", L_GetFramerate);
    // Addon/Binding stubs (kept for compatibility)
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

    // Set CURRENT_ACTIONBAR_PAGE global
    lua_pushnumber(L, 1);
    lua_setglobal(L, "CURRENT_ACTIONBAR_PAGE");

    UE_LOG(LogWowLuaStub, Log, TEXT("Registered WoW Lua API (~150+ functions, entity-backed unit API, FrameXML-ready)"));
}

#else
void WowLuaApi::RegisterStubs(lua_State*) {}
#endif
