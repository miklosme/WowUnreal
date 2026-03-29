#include "LuaApiRegistry.h"
#include "WowEntityManager.h"
#include "WowEntity.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowUpdateFields.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/SpellDbc.h"
#include "Formats/Dbc/SpellIconDbc.h"
#include "Formats/Dbc/TalentTabDbc.h"
#include "WowAddonLoader.h"
#include "Mpq/MpqManager.h"
#include "Mpq/MpqManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
// WowAudioManager is in WowWorld module — not directly accessible from WowUI
#include "WowFrameManager.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

DEFINE_LOG_CATEGORY_STATIC(LogWowLuaStub, Verbose, All);

// WoW 3.3.5a Combat Rating Types (from PlayerFields.h / azerothcore)
namespace WowCombatRating
{
    inline constexpr int32 WEAPON_SKILL = 0;
    inline constexpr int32 DEFENCE = 1;
    inline constexpr int32 DODGE = 2;
    inline constexpr int32 PARRY = 3;
    inline constexpr int32 BLOCK = 4;
    inline constexpr int32 HIT_MELEE = 5;
    inline constexpr int32 HIT_RANGED = 6;
    inline constexpr int32 HIT_SPELL = 7;
    inline constexpr int32 CRIT_MELEE = 8;
    inline constexpr int32 CRIT_RANGED = 9;
    inline constexpr int32 CRIT_SPELL = 10;
    inline constexpr int32 HIT_TAKEN_MELEE = 11;
    inline constexpr int32 HIT_TAKEN_RANGED = 12;
    inline constexpr int32 HIT_TAKEN_SPELL = 13;
    inline constexpr int32 CRIT_TAKEN_MELEE = 14;
    inline constexpr int32 CRIT_TAKEN_RANGED = 15;
    inline constexpr int32 CRIT_TAKEN_SPELL = 16;
    inline constexpr int32 HASTE_MELEE = 17;
    inline constexpr int32 HASTE_RANGED = 18;
    inline constexpr int32 HASTE_SPELL = 19;
    inline constexpr int32 HIT_TAKEN = 20;
    inline constexpr int32 CRIT_TAKEN = 21;
    inline constexpr int32 RESILIENCE = 22;
    inline constexpr int32 HASTE = 23;
    inline constexpr int32 EXPERTISE = 24;
}

// WoW 3.3.5a Unit Flags
namespace WowUnitFlags
{
    inline constexpr uint32 IN_COMBAT = 0x00080000;
}

// WoW 3.3.5a School mask for spells (for GetSpellCritChance)
namespace WowSpellSchool
{
    inline constexpr int32 PHYSICAL = 0;
    inline constexpr int32 HOLY = 1;
    inline constexpr int32 FIRE = 2;
    inline constexpr int32 NATURE = 3;
    inline constexpr int32 FROST = 4;
    inline constexpr int32 SHADOW = 5;
    inline constexpr int32 ARCANE = 6;
}

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

// Helper: resolve common WoW unit strings to entity GUID
static uint64 ResolveUnitGuid(lua_State* L, int ArgIdx = 1)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->EntityManager) return 0;

    const char* unit = luaL_optstring(L, ArgIdx, "player");

    if (strcmp(unit, "player") == 0)
    {
        return Ctx->EntityManager->LocalPlayerGuid;
    }
    else if (strcmp(unit, "target") == 0 && Ctx->ConnectionManager)
    {
        return static_cast<uint64>(Ctx->ConnectionManager->GetTargetGuid());
    }
    else if (strncmp(unit, "party", 5) == 0 && unit[5] >= '1' && unit[5] <= '4' && unit[6] == '\0')
    {
        if (Ctx->ConnectionManager)
        {
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            const int32 PartyIndex = unit[5] - '1';
            if (PartyIndex >= 0 && PartyIndex < GroupInfo.Members.Num())
            {
                return GroupInfo.Members[PartyIndex].Guid;
            }
        }
    }
    else if (strncmp(unit, "raid", 4) == 0 && unit[4] != '\0')
    {
        if (Ctx->ConnectionManager)
        {
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            if (!GroupInfo.IsRaidGroup())
            {
                return 0;
            }

            const int32 RaidIndex = FCStringAnsi::Atoi(unit + 4);
            if (RaidIndex <= 0)
            {
                return 0;
            }

            if (RaidIndex == 1)
            {
                return Ctx->EntityManager->LocalPlayerGuid;
            }

            const int32 MemberIndex = RaidIndex - 2;
            if (GroupInfo.Members.IsValidIndex(MemberIndex))
            {
                return GroupInfo.Members[MemberIndex].Guid;
            }
        }
    }
    else if (strcmp(unit, "focus") == 0)
    {
        // Focus target not implemented yet
        return 0;
    }
    else if (strcmp(unit, "pet") == 0)
    {
        // Pet not implemented yet
        return 0;
    }

    return 0;
}

// Helper: resolve unit string to entity (wraps ResolveUnitGuid for backwards compatibility)
static FWowEntity* ResolveUnit(lua_State* L, int ArgIdx = 1)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->EntityManager) return nullptr;

    uint64 Guid = ResolveUnitGuid(L, ArgIdx);
    return Guid != 0 ? Ctx->EntityManager->Find(Guid) : nullptr;
}

enum class EWowUnitRelationship : uint8
{
    Hostile,
    Neutral,
    Friendly,
};

static bool IsAllianceRace(uint8 RaceId)
{
    switch (RaceId)
    {
    case 1:  // Human
    case 3:  // Dwarf
    case 4:  // Night Elf
    case 7:  // Gnome
    case 11: // Draenei
        return true;
    default:
        return false;
    }
}

static bool IsHordeRace(uint8 RaceId)
{
    switch (RaceId)
    {
    case 2:  // Orc
    case 5:  // Undead
    case 6:  // Tauren
    case 8:  // Troll
    case 10: // Blood Elf
        return true;
    default:
        return false;
    }
}

static uint64 ResolveControllingGuid(const FWowEntity* Entity)
{
    if (!Entity)
    {
        return 0;
    }

    if (!Entity->IsUnit())
    {
        return Entity->Guid;
    }

    const FWowUnitEntity* Unit = static_cast<const FWowUnitEntity*>(Entity);
    const uint64 CharmedByGuid = Unit->GetField64(UnitField::CHARMEDBY);
    if (CharmedByGuid != 0)
    {
        return CharmedByGuid;
    }

    const uint64 SummonedByGuid = Unit->GetField64(UnitField::SUMMONEDBY);
    if (SummonedByGuid != 0)
    {
        return SummonedByGuid;
    }

    const uint64 CreatedByGuid = Unit->GetField64(UnitField::CREATEDBY);
    if (CreatedByGuid != 0)
    {
        return CreatedByGuid;
    }

    return Entity->Guid;
}

static bool IsGuidInPlayerGroup(const FWowLuaContext* Ctx, uint64 Guid)
{
    if (!Ctx || !Ctx->EntityManager || Guid == 0)
    {
        return false;
    }

    if (Guid == Ctx->EntityManager->LocalPlayerGuid)
    {
        return true;
    }

    if (!Ctx->ConnectionManager)
    {
        return false;
    }

    const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
    return GroupInfo.FindMember(Guid) != nullptr;
}

static int32 GetPlayerTeam(const FWowLuaContext* Ctx, uint64 Guid)
{
    if (!Ctx || !Ctx->EntityManager || Guid == 0)
    {
        return 0;
    }

    const FWowEntity* Entity = Ctx->EntityManager->Find(Guid);
    if (!Entity || !Entity->IsPlayer())
    {
        return 0;
    }

    const FWowUnitEntity* Unit = static_cast<const FWowUnitEntity*>(Entity);
    const uint8 RaceId = Unit->GetRaceId();
    if (IsAllianceRace(RaceId))
    {
        return 1;
    }

    if (IsHordeRace(RaceId))
    {
        return 2;
    }

    return 0;
}

static EWowUnitRelationship GetUnitRelationship(const FWowLuaContext* Ctx, const FWowEntity* Unit1, const FWowEntity* Unit2)
{
    if (!Ctx || !Unit1 || !Unit2)
    {
        return EWowUnitRelationship::Neutral;
    }

    const uint64 ControllerGuid1 = ResolveControllingGuid(Unit1);
    const uint64 ControllerGuid2 = ResolveControllingGuid(Unit2);

    if (ControllerGuid1 != 0 && ControllerGuid1 == ControllerGuid2)
    {
        return EWowUnitRelationship::Friendly;
    }

    if (IsGuidInPlayerGroup(Ctx, ControllerGuid1) && IsGuidInPlayerGroup(Ctx, ControllerGuid2))
    {
        return EWowUnitRelationship::Friendly;
    }

    const int32 Team1 = GetPlayerTeam(Ctx, ControllerGuid1);
    const int32 Team2 = GetPlayerTeam(Ctx, ControllerGuid2);
    if (Team1 != 0 && Team2 != 0)
    {
        return Team1 == Team2 ? EWowUnitRelationship::Friendly : EWowUnitRelationship::Hostile;
    }

    if (Unit1->IsUnit() && Unit2->IsUnit())
    {
        const uint32 FactionTemplate1 = static_cast<const FWowUnitEntity*>(Unit1)->GetFactionTemplate();
        const uint32 FactionTemplate2 = static_cast<const FWowUnitEntity*>(Unit2)->GetFactionTemplate();

        if (FactionTemplate1 != 0 && FactionTemplate2 != 0)
        {
            if (FactionTemplate1 == FactionTemplate2)
            {
                return EWowUnitRelationship::Friendly;
            }

            if (FactionTemplate1 == 2000 || FactionTemplate2 == 2000)
            {
                return EWowUnitRelationship::Neutral;
            }
        }

        const bool bUnit1IsPlayerSide = IsGuidInPlayerGroup(Ctx, ControllerGuid1) || Team1 != 0;
        const bool bUnit2IsPlayerSide = IsGuidInPlayerGroup(Ctx, ControllerGuid2) || Team2 != 0;
        if (bUnit1IsPlayerSide != bUnit2IsPlayerSide)
        {
            const uint32 OtherFactionTemplate = bUnit1IsPlayerSide ? FactionTemplate2 : FactionTemplate1;
            if (OtherFactionTemplate == 2000)
            {
                return EWowUnitRelationship::Neutral;
            }

            if (OtherFactionTemplate > 100)
            {
                return EWowUnitRelationship::Hostile;
            }
        }
    }

    return EWowUnitRelationship::Neutral;
}

static int32 GetUnitReactionRank(const FWowLuaContext* Ctx, const FWowEntity* Unit1, const FWowEntity* Unit2)
{
    switch (GetUnitRelationship(Ctx, Unit1, Unit2))
    {
    case EWowUnitRelationship::Friendly:
        return 8;
    case EWowUnitRelationship::Hostile:
        return 2;
    default:
        return 4;
    }
}

// ─── Instance / Group ───────────────────────────────────────────────────────────
static int L_IsInInstance(lua_State* L)
{
    // Return false, "none" for now (not in instance, no instance type)
    lua_pushboolean(L, false);
    lua_pushstring(L, "none");
    return 2;
}

static int L_GetNumPartyMembers(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
        if (!GroupInfo.IsEmpty() && !GroupInfo.IsRaidGroup())
        {
            lua_pushnumber(L, FMath::Max(0, static_cast<int32>(GroupInfo.MemberCount) - 1));
            return 1;
        }
    }

    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetNumRaidMembers(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
        if (!GroupInfo.IsEmpty() && GroupInfo.IsRaidGroup())
        {
            lua_pushnumber(L, GroupInfo.MemberCount);
            return 1;
        }
    }

    lua_pushnumber(L, 0);
    return 1;
}

static int L_IsInGuild(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const FString& GuildName = Ctx->ConnectionManager->PacketHandler.GuildName;
        lua_pushboolean(L, !GuildName.IsEmpty() ? 1 : 0);
        return 1;
    }

    lua_pushboolean(L, 0);
    return 1;
}
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
static int L_InCombatLockdown(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint32 UnitFlags = Player->GetUnitFlags();
            // Check UNIT_FLAG_IN_COMBAT (0x00080000)
            bool InCombat = (UnitFlags & 0x80000) != 0;
            lua_pushboolean(L, InCombat ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int L_IsResting(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowEntity* LocalPlayer = Ctx->EntityManager->GetLocalPlayer();
        if (LocalPlayer)
        {
            // Check PLAYER_FLAGS for PLAYER_FLAGS_RESTING (0x0002)
            FWowPlayerEntity PlayerEntity(*LocalPlayer);
            uint32 PlayerFlags = PlayerEntity.GetPlayerFlags();
            bool IsResting = (PlayerFlags & 0x0002) != 0;
            lua_pushboolean(L, IsResting);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

static int L_IsMounted(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint32 MountDisplayId = Player->GetMountDisplayId();
            lua_pushboolean(L, MountDisplayId > 0 ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int L_IsFlying(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint32 MoveFlags = Player->Movement.MoveFlags;
            // Check MOVEMENTFLAG_FLYING (0x800000)
            bool IsFlying = (MoveFlags & 0x800000) != 0;
            lua_pushboolean(L, IsFlying ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int L_IsSwimming(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint32 MoveFlags = Player->Movement.MoveFlags;
            // Check MOVEMENTFLAG_SWIMMING (0x200000)
            bool IsSwimming = (MoveFlags & 0x200000) != 0;
            lua_pushboolean(L, IsSwimming ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int L_IsFalling(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint32 MoveFlags = Player->Movement.MoveFlags;
            // Check MOVEMENTFLAG_FALLING (0x2)
            bool IsFalling = (MoveFlags & 0x2) != 0;
            lua_pushboolean(L, IsFalling ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int L_IsStealthed(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // Check for stealth auras (basic stealth spell IDs)
            const TArray<uint32> StealthSpellIds = { 1784, 5215, 58984 }; // Stealth, Prowl, Shadow Meld
            for (const FAuraInfo& Aura : Player->Auras)
            {
                if (Aura.bActive && StealthSpellIds.Contains(Aura.SpellId))
                {
                    lua_pushboolean(L, 1);
                    return 1;
                }
            }
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

// GetPlayerMapPosition("player") → x, y (map coordinates 0.0-1.0)
static int L_GetPlayerMapPosition(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    const char* Unit = luaL_optstring(L, 1, "player");

    if (Ctx && Ctx->EntityManager)
    {
        FWowEntity* Entity = ResolveUnit(L);
        if (Entity)
        {
            // Get world position from movement info
            const FVector& Position = Entity->Movement.Position;

            // TODO: Convert world coordinates to proper map coordinates (0.0-1.0)
            // For now, normalize assuming a standard map size
            // WoW maps are typically ~34000x34000 world units
            float MapX = (Position.X + 17066.0f) / 34132.0f;
            float MapY = (Position.Y + 17066.0f) / 34132.0f;

            // Clamp to valid range
            MapX = FMath::Clamp(MapX, 0.0f, 1.0f);
            MapY = FMath::Clamp(MapY, 0.0f, 1.0f);

            lua_pushnumber(L, MapX);
            lua_pushnumber(L, MapY);
            return 2;
        }
    }

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

    uint64 Guid = ResolveUnitGuid(L);
    if (Guid != 0 && Ctx && Ctx->ConnectionManager)
    {
        // Check player name cache first
        if (const FString* PlayerName = Ctx->ConnectionManager->PacketHandler.PlayerNameCache.Find(Guid))
        {
            lua_pushstring(L, TCHAR_TO_UTF8(**PlayerName));
            return 1;
        }

        // For NPCs, look up by entry ID in creature name cache
        FWowEntity* Entity = Ctx->EntityManager->Find(Guid);
        if (Entity && !Entity->IsPlayer())
        {
            if (const FString* CreatureName = Ctx->ConnectionManager->PacketHandler.CreatureNameCache.Find(Entity->Entry))
            {
                lua_pushstring(L, TCHAR_TO_UTF8(**CreatureName));
                return 1;
            }
        }

        // Fallback for unknown entities
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
    int32 PowerType = static_cast<int32>(luaL_optnumber(L, 2, 0)); // 0=mana, 1=rage, 2=focus, 3=energy, etc.

    if (Entity && Entity->IsUnit())
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        lua_pushnumber(L, Unit->GetPower(static_cast<uint8>(PowerType)));
    }
    else
    {
        lua_pushnumber(L, 0);
    }
    return 1;
}

static int L_UnitPowerMax(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    int32 PowerType = static_cast<int32>(luaL_optnumber(L, 2, 0)); // 0=mana, 1=rage, 2=focus, 3=energy, etc.

    if (Entity && Entity->IsUnit())
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        lua_pushnumber(L, Unit->GetMaxPower(static_cast<uint8>(PowerType)));
    }
    else
    {
        lua_pushnumber(L, 0);
    }
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
    uint64 Guid = ResolveUnitGuid(L);
    lua_pushboolean(L, Guid != 0 ? 1 : 0);
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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    FWowEntity* Unit1 = nullptr;
    FWowEntity* Unit2 = nullptr;

    if (lua_gettop(L) >= 2)
    {
        Unit1 = ResolveUnit(L, 1);
        Unit2 = ResolveUnit(L, 2);
    }
    else
    {
        Unit1 = Ctx && Ctx->EntityManager ? Ctx->EntityManager->Find(Ctx->EntityManager->LocalPlayerGuid) : nullptr;
        Unit2 = ResolveUnit(L, 1);
    }

    lua_pushboolean(L, GetUnitRelationship(Ctx, Unit1, Unit2) == EWowUnitRelationship::Friendly ? 1 : 0);
    return 1;
}

static int L_UnitIsEnemy(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    FWowEntity* Unit1 = nullptr;
    FWowEntity* Unit2 = nullptr;

    if (lua_gettop(L) >= 2)
    {
        Unit1 = ResolveUnit(L, 1);
        Unit2 = ResolveUnit(L, 2);
    }
    else
    {
        Unit1 = Ctx && Ctx->EntityManager ? Ctx->EntityManager->Find(Ctx->EntityManager->LocalPlayerGuid) : nullptr;
        Unit2 = ResolveUnit(L, 1);
    }

    lua_pushboolean(L, GetUnitRelationship(Ctx, Unit1, Unit2) == EWowUnitRelationship::Hostile ? 1 : 0);
    return 1;
}

static int L_UnitAffectingCombat(lua_State* L)
{
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity && Entity->IsUnit())
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        uint32 UnitFlags = Unit->GetUnitFlags();

        // Check UNIT_FLAG_IN_COMBAT flag
        bool InCombat = (UnitFlags & WowUnitFlags::IN_COMBAT) != 0;
        lua_pushboolean(L, InCombat ? 1 : 0);
        return 1;
    }

    lua_pushboolean(L, 0);
    return 1;
}

static int L_UnitBuff(lua_State* L)
{
    // UnitBuff(unit, index) → name, rank, icon, count, debuffType, duration, expirationTime, isFromPlayer, isStealable
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    FWowEntity* Entity = ResolveUnit(L);
    int32 Index = static_cast<int32>(luaL_checknumber(L, 2)) - 1; // Convert 1-based to 0-based

    if (Entity && Index >= 0 && Index < Entity->Auras.Num() && Entity->Auras[Index].bActive)
    {
        const FAuraInfo& Aura = Entity->Auras[Index];

        // Look up spell info from DBC
        if (Ctx && Aura.SpellId != 0)
        {
            const FSpellDbcEntry* SpellEntry = FDbcStore::Get().Spells().GetById(Aura.SpellId);
            if (SpellEntry)
            {
                lua_pushstring(L, TCHAR_TO_UTF8(*SpellEntry->SpellName)); // name
                lua_pushstring(L, TCHAR_TO_UTF8(*SpellEntry->Rank)); // rank
                lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark"); // icon (placeholder)
                lua_pushnumber(L, Aura.Charges > 0 ? Aura.Charges : 1); // count
                lua_pushstring(L, "Magic"); // debuffType (TODO: determine from spell data)
                lua_pushnumber(L, 0); // duration (TODO: implement duration tracking)
                lua_pushnumber(L, 0); // expirationTime
                lua_pushboolean(L, Entity->IsPlayer() && Aura.CasterGuid == Entity->Guid ? 1 : 0); // isFromPlayer
                lua_pushboolean(L, 0); // isStealable (TODO: determine from spell data)
                return 9;
            }
        }
    }

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
    // UnitDebuff works the same as UnitBuff for now
    // TODO: Implement proper buff/debuff filtering based on aura flags
    return L_UnitBuff(L);
}

static int L_UnitAura(lua_State* L)
{
    // UnitAura(unit, index, filter) → name, rank, icon, count, debuffType, duration, expirationTime, isFromPlayer, isStealable
    // For now, same as UnitBuff - could implement filter logic later
    return L_UnitBuff(L);
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
        uint8 PowerType = Unit->GetPowerTypeId();

        // Map power type ID to string
        const char* PowerTypeName = "MANA";
        switch (PowerType)
        {
        case 0: PowerTypeName = "MANA"; break;
        case 1: PowerTypeName = "RAGE"; break;
        case 2: PowerTypeName = "FOCUS"; break;
        case 3: PowerTypeName = "ENERGY"; break;
        case 4: PowerTypeName = "HAPPINESS"; break; // Pet happiness
        case 6: PowerTypeName = "RUNIC_POWER"; break; // Death Knight
        default: PowerTypeName = "MANA"; break;
        }

        lua_pushnumber(L, PowerType);
        lua_pushstring(L, PowerTypeName);
        return 2;
    }
    lua_pushnumber(L, 0);
    lua_pushstring(L, "MANA");
    return 2;
}

static int L_UnitStat(lua_State* L)
{
    // UnitStat(unit, statIndex) → base, stat, posBuff, negBuff
    // statIndex: 1=Strength, 2=Agility, 3=Stamina, 4=Intellect, 5=Spirit
    FWowEntity* Entity = ResolveUnit(L);
    int32 StatIndex = static_cast<int32>(luaL_checknumber(L, 2));

    if (Entity && Entity->IsUnit() && StatIndex >= 1 && StatIndex <= 5)
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        uint32 StatValue = 0;

        switch (StatIndex)
        {
        case 1: StatValue = Unit->GetStrength(); break;
        case 2: StatValue = Unit->GetAgility(); break;
        case 3: StatValue = Unit->GetStamina(); break;
        case 4: StatValue = Unit->GetIntellect(); break;
        case 5: StatValue = Unit->GetSpirit(); break;
        }

        // For now, return the total stat as both base and current
        // TODO: Implement stat calculation to split base/buffs/debuffs
        lua_pushnumber(L, StatValue); // base
        lua_pushnumber(L, StatValue); // stat
        lua_pushnumber(L, 0); // posBuff
        lua_pushnumber(L, 0); // negBuff
    }
    else
    {
        lua_pushnumber(L, 10); // base
        lua_pushnumber(L, 10); // stat
        lua_pushnumber(L, 0); // posBuff
        lua_pushnumber(L, 0); // negBuff
    }
    return 4;
}

static int L_UnitAttackPower(lua_State* L)
{
    // UnitAttackPower(unit) → base, posBuff, negBuff
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity && Entity->IsUnit())
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        int32 AttackPower = Unit->GetAttackPower();

        lua_pushnumber(L, AttackPower); // base
        lua_pushnumber(L, 0); // posBuff (TODO: implement buff/debuff calculation)
        lua_pushnumber(L, 0); // negBuff
    }
    else
    {
        lua_pushnumber(L, 100); // base
        lua_pushnumber(L, 0); // posBuff
        lua_pushnumber(L, 0); // negBuff
    }
    return 3;
}

static int L_UnitRangedAttackPower(lua_State* L)
{
    // UnitRangedAttackPower(unit) → base, posBuff, negBuff
    FWowEntity* Entity = ResolveUnit(L);
    if (Entity && Entity->IsUnit())
    {
        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        int32 RangedAttackPower = Unit->GetRangedAttackPower();

        lua_pushnumber(L, RangedAttackPower); // base
        lua_pushnumber(L, 0); // posBuff (TODO: implement buff/debuff calculation)
        lua_pushnumber(L, 0); // negBuff
    }
    else
    {
        lua_pushnumber(L, 50); // base
        lua_pushnumber(L, 0); // posBuff
        lua_pushnumber(L, 0); // negBuff
    }
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
        // Get realm name from the currently connected realm
        const TArray<FWowRealmInfo>& CachedRealms = Ctx->ConnectionManager->GetCachedRealms();
        if (CachedRealms.Num() > 0)
        {
            // For now, return the first cached realm name
            // In a full implementation, we'd track which realm was selected
            lua_pushstring(L, TCHAR_TO_UTF8(*CachedRealms[0].Name));
        }
        else
        {
            lua_pushstring(L, "WowUnreal");
        }
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
// WoW returns screen dimensions in UI coordinate units (height is always 768 base,
// width varies with aspect ratio).  Addons use these to position frames correctly.
static int L_GetScreenWidth(lua_State* L)
{
    FVector2D Size(1920, 1080);
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(Size);
    }
    // Convert to WoW UI units: width = viewport_width / (viewport_height / 768)
    float UIScale = Size.Y / 768.0f;
    lua_pushnumber(L, Size.X / UIScale);
    return 1;
}

static int L_GetScreenHeight(lua_State* L)
{
    // WoW's base UI height is always 768 units
    lua_pushnumber(L, 768.0);
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

// ─── Action bar functions ───────────────────────────────────────────────────────
// Helper: safely read action slot from arg 1, returns -1 if nil
static int32 SafeActionSlot(lua_State* L)
{
    if (lua_isnil(L, 1) || lua_isnone(L, 1)) return -1;
    return static_cast<int32>(lua_tonumber(L, 1)) - 1; // Convert 1-based to 0-based
}

static bool ResolveSpellBookSpellId(lua_State* L, int32 SpellIndexOrId, const char* BookType, uint32& OutSpellId)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->ConnectionManager || strcmp(BookType, "spell") != 0)
    {
        return false;
    }

    const TSet<uint32>& KnownSpells = Ctx->ConnectionManager->PacketHandler.KnownSpells;
    if (KnownSpells.Contains(static_cast<uint32>(SpellIndexOrId)))
    {
        OutSpellId = static_cast<uint32>(SpellIndexOrId);
        return true;
    }

    if (SpellIndexOrId <= 0 || SpellIndexOrId > KnownSpells.Num())
    {
        return false;
    }

    TArray<uint32> SpellArray = KnownSpells.Array();
    SpellArray.Sort();
    OutSpellId = SpellArray[SpellIndexOrId - 1];
    return true;
}

static int L_PickupAction(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    const int32 Slot = SafeActionSlot(L);
    if (Ctx && Ctx->ConnectionManager && Slot >= 0)
    {
        Ctx->ConnectionManager->PickupActionCursor(Slot);
    }
    return 0;
}

static int L_PlaceAction(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    const int32 Slot = SafeActionSlot(L);
    if (Ctx && Ctx->ConnectionManager && Slot >= 0)
    {
        Ctx->ConnectionManager->PlaceCursorIntoActionSlot(Slot);
    }
    return 0;
}

static int L_PickupSpell(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    const int32 SpellIndexOrId = static_cast<int32>(luaL_checknumber(L, 1));
    const char* BookType = luaL_optstring(L, 2, "spell");
    uint32 SpellId = 0;

    if (Ctx && Ctx->ConnectionManager && ResolveSpellBookSpellId(L, SpellIndexOrId, BookType, SpellId))
    {
        Ctx->ConnectionManager->PickupSpellCursor(static_cast<int32>(SpellId), UTF8_TO_TCHAR(BookType));
    }

    return 0;
}

static int L_GetCursorInfo(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    FString Type;
    FString Detail;
    int32 Id = 0;

    if (Ctx && Ctx->ConnectionManager && Ctx->ConnectionManager->GetCursorInfo(Type, Id, Detail))
    {
        lua_pushstring(L, TCHAR_TO_UTF8(*Type));
        lua_pushnumber(L, Id);
        if (!Detail.IsEmpty())
        {
            lua_pushstring(L, TCHAR_TO_UTF8(*Detail));
            return 3;
        }
        return 2;
    }

    return 0;
}

static int L_ClearCursor(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        Ctx->ConnectionManager->ClearCursorPayload();
    }
    return 0;
}

static int L_CursorHasSpell(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    const bool bHasSpell = Ctx && Ctx->ConnectionManager && Ctx->ConnectionManager->HasCursorSpellPayload();
    lua_pushboolean(L, bHasSpell);
    return 1;
}

static FString GetModifiedClickBindingName(const char* Action)
{
    if (strcmp(Action, "CHATLINK") == 0) return TEXT("SHIFT");
    if (strcmp(Action, "SELFCAST") == 0) return TEXT("ALT");
    if (strcmp(Action, "FOCUSCAST") == 0) return TEXT("CTRL");
    if (strcmp(Action, "AUTOLOOTTOGGLE") == 0) return TEXT("SHIFT");
    if (strcmp(Action, "COMPAREITEMS") == 0) return TEXT("SHIFT");
    if (strcmp(Action, "OPENALLBAGS") == 0) return TEXT("SHIFT");
    if (strcmp(Action, "QUESTWATCHTOGGLE") == 0) return TEXT("SHIFT");
    if (strcmp(Action, "DRESSUP") == 0) return TEXT("CTRL");
    if (strcmp(Action, "PICKUPACTION") == 0) return TEXT("SHIFT");
    return TEXT("NONE");
}

static bool IsModifierBindingPressed(const FString& BindingName)
{
    if (!FSlateApplication::IsInitialized())
    {
        return false;
    }

    const FModifierKeysState Modifiers = FSlateApplication::Get().GetModifierKeys();
    if (BindingName == TEXT("SHIFT")) return Modifiers.IsShiftDown();
    if (BindingName == TEXT("CTRL")) return Modifiers.IsControlDown();
    if (BindingName == TEXT("ALT")) return Modifiers.IsAltDown();
    return false;
}

static int L_GetModifiedClick(lua_State* L)
{
    const char* Action = luaL_optstring(L, 1, "");
    const FString BindingName = GetModifiedClickBindingName(Action);
    lua_pushstring(L, TCHAR_TO_UTF8(*BindingName));
    return 1;
}

static int L_IsModifiedClick(lua_State* L)
{
    if (lua_isnoneornil(L, 1))
    {
        bool bAnyModifier = false;
        if (FSlateApplication::IsInitialized())
        {
            const FModifierKeysState Modifiers = FSlateApplication::Get().GetModifierKeys();
            bAnyModifier = Modifiers.IsShiftDown() || Modifiers.IsControlDown() || Modifiers.IsAltDown();
        }
        lua_pushboolean(L, bAnyModifier);
        return 1;
    }

    const char* Action = luaL_optstring(L, 1, "");
    const FString BindingName = GetModifiedClickBindingName(Action);
    lua_pushboolean(L, IsModifierBindingPressed(BindingName));
    return 1;
}

static int L_HasAction(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            lua_pushboolean(L, 1);
            return 1;
        }
    }

    lua_pushboolean(L, 0);
    return 1;
}

static int L_GetActionInfo(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            uint32 ActionData = ActionButtons[Slot];
            uint8 ActionType = (ActionData >> 24) & 0xFF;
            uint32 ActionId = ActionData & 0x00FFFFFF;

            switch (ActionType)
            {
            case 0: // Spell
                lua_pushstring(L, "spell");
                lua_pushnumber(L, ActionId);
                return 2;
            case 64: // Macro
                lua_pushstring(L, "macro");
                lua_pushnumber(L, ActionId);
                return 2;
            case 128: // Item
                lua_pushstring(L, "item");
                lua_pushnumber(L, ActionId);
                return 2;
            default:
                lua_pushstring(L, "unknown");
                lua_pushnumber(L, ActionId);
                return 2;
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

static int L_GetActionTexture(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (lua_isnil(L, 1) || lua_isnone(L, 1)) { lua_pushnil(L); return 1; }
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            uint32 ActionData = ActionButtons[Slot];
            uint8 ActionType = (ActionData >> 24) & 0xFF;
            uint32 ActionId = ActionData & 0x00FFFFFF;

            if (ActionType == 0) // Spell
            {
                // Look up spell icon from SpellIcon.dbc
                const FSpellDbcEntry* SpellEntry = FDbcStore::Get().Spells().GetById(ActionId);
                if (SpellEntry)
                {
                    const FSpellIconDbcEntry* IconEntry = FDbcStore::Get().SpellIcons().GetById(SpellEntry->SpellIconID);
                    if (IconEntry && !IconEntry->TexturePath.IsEmpty())
                    {
                        UE_LOG(LogWowLuaStub, Verbose, TEXT("GetActionTexture: slot %d spell %d icon %d -> %s"),
                            Slot+1, ActionId, SpellEntry->SpellIconID, *IconEntry->TexturePath);
                        lua_pushstring(L, TCHAR_TO_UTF8(*IconEntry->TexturePath));
                        return 1;
                    }
                    UE_LOG(LogWowLuaStub, Warning, TEXT("GetActionTexture: spell %d iconID %d not found in SpellIcon.dbc"),
                        ActionId, SpellEntry->SpellIconID);
                }
                else
                {
                    UE_LOG(LogWowLuaStub, Warning, TEXT("GetActionTexture: spell %d not found in Spell.dbc"), ActionId);
                }
                lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark");
                return 1;
            }
            else if (ActionType == 128) // Item
            {
                // TODO: Look up item icon from Item.dbc
                lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark");
                return 1;
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

static int L_UseAction(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            uint32 ActionData = ActionButtons[Slot];
            uint8 ActionType = (ActionData >> 24) & 0xFF;
            uint32 ActionId = ActionData & 0x00FFFFFF;

            if (ActionType == 0) // Spell
            {
                int64 TargetGuid = Ctx->ConnectionManager->GetTargetGuid();
                Ctx->ConnectionManager->SendCastSpell(ActionId, TargetGuid);
            }
            // TODO: Implement item usage for ActionType == 128
        }
    }

    return 0;
}

static int L_GetActionCount(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            uint32 ActionData = ActionButtons[Slot];
            uint8 ActionType = (ActionData >> 24) & 0xFF;
            uint32 ActionId = ActionData & 0x00FFFFFF;

            if (ActionType == 128) // Item
            {
                // Find the item in player's inventory and return its stack count
                if (Ctx->EntityManager)
                {
                    FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
                    if (Player)
                    {
                        // Check backpack slots (0-15)
                        for (uint8 SlotIndex = 0; SlotIndex < 16; ++SlotIndex)
                        {
                            uint64 ItemGuid = Player->GetBackpackItemGuid(SlotIndex);
                            if (ItemGuid != 0)
                            {
                                FWowItemEntity* Item = Ctx->EntityManager->FindItem(ItemGuid);
                                if (Item && Item->Entry == ActionId)
                                {
                                    lua_pushnumber(L, Item->GetStackCount());
                                    return 1;
                                }
                            }
                        }

                        // TODO: Check bag slots as well for complete implementation
                    }
                }
            }
        }
    }

    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetActionCooldown(lua_State* L)
{
    // GetActionCooldown(slot) → start, duration, enabled
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            uint32 ActionData = ActionButtons[Slot];
            uint8 ActionType = (ActionData >> 24) & 0xFF;
            uint32 ActionId = ActionData & 0x00FFFFFF;

            if (ActionType == 0) // Spell
            {
                // Check spell cooldown
                const FWowPacketHandler& Handler = Ctx->ConnectionManager->PacketHandler;
                float RemainingTime = Handler.GetSpellCooldownRemaining(ActionId);

                if (RemainingTime > 0.0f)
                {
                    double CurrentTime = FPlatformTime::Seconds();
                    double StartTime = CurrentTime - RemainingTime;
                    lua_pushnumber(L, StartTime); // start time
                    lua_pushnumber(L, RemainingTime); // duration
                    lua_pushnumber(L, 1); // enabled
                    return 3;
                }
            }
        }
    }

    lua_pushnumber(L, 0); // start
    lua_pushnumber(L, 0); // duration
    lua_pushnumber(L, 1); // enabled
    return 3;
}

static int L_IsCurrentAction(lua_State* L)
{
    // TODO: Check if this is the currently active/channeling action
    lua_pushboolean(L, 0);
    return 1;
}

static int L_IsUsableAction(lua_State* L)
{
    // IsUsableAction(slot) → usable, noMana
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            uint32 ActionData = ActionButtons[Slot];
            uint8 ActionType = (ActionData >> 24) & 0xFF;
            uint32 ActionId = ActionData & 0x00FFFFFF;

            if (ActionType == 0) // Spell
            {
                // Check if player knows the spell
                const FWowPacketHandler& Handler = Ctx->ConnectionManager->PacketHandler;
                if (Handler.KnownSpells.Contains(ActionId))
                {
                    // Check mana/resource requirements for the spell
                    bool bHasResource = true;
                    bool bNoMana = false;

                    if (Ctx->EntityManager)
                    {
                        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
                        if (Player)
                        {
                            // For basic implementation, check if player has mana for spells
                            // In a full implementation, we'd need spell cost from DBC
                            int32 CurrentMana = Player->GetPower(0); // Power type 0 = Mana
                            if (CurrentMana <= 0)
                            {
                                bHasResource = false;
                                bNoMana = true;
                            }
                        }
                    }

                    lua_pushboolean(L, bHasResource ? 1 : 0); // usable
                    lua_pushboolean(L, bNoMana ? 1 : 0); // noMana
                    return 2;
                }
            }
        }
    }

    lua_pushboolean(L, 1);
    lua_pushboolean(L, 0);
    return 2;
}

static int L_IsAttackAction(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Slot = SafeActionSlot(L);

    if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
    {
        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
        {
            uint32 ActionData = ActionButtons[Slot];
            uint8 ActionType = (ActionData >> 24) & 0xFF;
            uint32 ActionId = ActionData & 0x00FFFFFF;

            if (ActionType == 0 && ActionId == 6603) // Attack spell ID in WoW
            {
                lua_pushboolean(L, 1);
                return 1;
            }
        }
    }

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
    const FSpellDbcEntry* SpellEntry = FDbcStore::Get().Spells().GetById(SpellId);
    if (SpellEntry)
    {
        lua_pushstring(L, TCHAR_TO_UTF8(*SpellEntry->SpellName));  // name
        lua_pushstring(L, TCHAR_TO_UTF8(*SpellEntry->Rank));        // rank

        // Look up icon from SpellIcon.dbc
        const FSpellIconDbcEntry* IconEntry = FDbcStore::Get().SpellIcons().GetById(SpellEntry->SpellIconID);
        if (IconEntry)
        {
            lua_pushstring(L, TCHAR_TO_UTF8(*IconEntry->TexturePath)); // icon
        }
        else
        {
            lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark"); // fallback icon
        }

        lua_pushnumber(L, 0);  // castTime (would need CastingTime DBC lookup)
        lua_pushnumber(L, 0);  // minRange (would need SpellRange DBC lookup)
        lua_pushnumber(L, 0);  // maxRange
        return 6;
    }
    lua_pushnil(L);
    return 1;
}

static int L_GetSpellCooldown(lua_State* L)
{
    // GetSpellCooldown(spellId) → start, duration, enabled
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 SpellId = static_cast<int32>(luaL_checknumber(L, 1));

    if (Ctx && Ctx->ConnectionManager)
    {
        const FWowPacketHandler& Handler = Ctx->ConnectionManager->PacketHandler;
        float RemainingTime = Handler.GetSpellCooldownRemaining(SpellId);

        if (RemainingTime > 0.0f)
        {
            // Spell is on cooldown
            double CurrentTime = FPlatformTime::Seconds();
            double StartTime = CurrentTime - RemainingTime;
            lua_pushnumber(L, StartTime); // start time
            lua_pushnumber(L, RemainingTime); // duration
            lua_pushnumber(L, 1); // enabled
        }
        else
        {
            // Spell is not on cooldown
            lua_pushnumber(L, 0); // start
            lua_pushnumber(L, 0); // duration
            lua_pushnumber(L, 1); // enabled
        }
    }
    else
    {
        lua_pushnumber(L, 0); // start
        lua_pushnumber(L, 0); // duration
        lua_pushnumber(L, 1); // enabled
    }
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
    int32 SpellId = static_cast<int32>(luaL_checknumber(L, 1));
    const FSpellDbcEntry* SpellEntry = FDbcStore::Get().Spells().GetById(SpellId);
    if (SpellEntry)
    {
        const FSpellIconDbcEntry* IconEntry = FDbcStore::Get().SpellIcons().GetById(SpellEntry->SpellIconID);
        if (IconEntry)
        {
            lua_pushstring(L, TCHAR_TO_UTF8(*IconEntry->TexturePath));
            return 1;
        }
    }
    lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark");
    return 1;
}

static int L_GetNumSpellTabs(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint8 ClassId = Player->GetClassId();
            // Each class has different talent tabs, but spell books typically have 2-4 tabs
            // For simplicity, return 3 for most classes
            lua_pushnumber(L, 3);
            return 1;
        }
    }
    lua_pushnumber(L, 3);
    return 1;
}

static int L_GetSpellTabInfo(lua_State* L)
{
    // GetSpellTabInfo(index) → name, texture, offset, numSpells
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 TabIndex = static_cast<int32>(luaL_checknumber(L, 1));

    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint8 ClassId = Player->GetClassId();

            // Get talent tabs for this class from TalentTab DBC
            const FTalentTabDbc& TalentTabs = FDbcStore::Get().TalentTabs();
            int32 TabCount = 0;
            for (const FTalentTabDbcEntry& TabEntry : TalentTabs.GetAll())
            {
                uint32 ClassMask = 1u << (ClassId - 1);
                if ((TabEntry.ClassMask & ClassMask) != 0)
                {
                    if (TabCount == TabIndex - 1) // Convert 1-based to 0-based
                    {
                        lua_pushstring(L, TCHAR_TO_UTF8(*TabEntry.Name)); // name
                        FString TexturePath = FString::Printf(TEXT("Interface\\Icons\\Spell_Icon_%d"), TabEntry.SpellIcon);
                        lua_pushstring(L, TCHAR_TO_UTF8(*TexturePath)); // texture
                        lua_pushnumber(L, TabCount * 12); // offset (approximate)
                        lua_pushnumber(L, 12); // numSpells (approximate)
                        return 4;
                    }
                    TabCount++;
                }
            }
        }
    }

    // Fallback for unknown tabs
    const char* TabNames[] = { "General", "Shadow", "Holy" };
    const char* TabTextures[] = { "Interface\\Icons\\Spell_Holy_MagicalSentry",
                                  "Interface\\Icons\\Spell_Shadow_ShadowWordPain",
                                  "Interface\\Icons\\Spell_Holy_HolyBolt" };

    if (TabIndex >= 1 && TabIndex <= 3)
    {
        lua_pushstring(L, TabNames[TabIndex - 1]); // name
        lua_pushstring(L, TabTextures[TabIndex - 1]); // texture
        lua_pushnumber(L, (TabIndex - 1) * 12); // offset
        lua_pushnumber(L, 12); // numSpells
        return 4;
    }

    lua_pushstring(L, "");
    lua_pushstring(L, "");
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 4;
}

static int L_IsUsableSpell(lua_State* L)
{
    // IsUsableSpell(spell) → usable, noMana
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const char* SpellName = luaL_optstring(L, 1, "");
        if (SpellName && strlen(SpellName) > 0)
        {
            // TODO: Look up spell ID by name from SpellDBC
            // For now, check if it's a known spell by name comparison
            const TSet<uint32>& KnownSpells = Ctx->ConnectionManager->PacketHandler.KnownSpells;

            // Simple check - assume spell is usable if we have any spells
            bool bUsable = !KnownSpells.IsEmpty();
            bool bNoMana = false; // TODO: Check if player has enough mana/power

            lua_pushboolean(L, bUsable ? 1 : 0);
            lua_pushboolean(L, bNoMana ? 1 : 0);
            return 2;
        }
    }

    lua_pushboolean(L, 0);
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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Index = static_cast<int32>(luaL_checknumber(L, 1));
    const char* BookType = luaL_optstring(L, 2, "spell");

    if (Ctx && Ctx->ConnectionManager && strcmp(BookType, "spell") == 0)
    {
        const TSet<uint32>& KnownSpells = Ctx->ConnectionManager->PacketHandler.KnownSpells;

        if (Index > 0 && Index <= KnownSpells.Num())
        {
            // Convert set to array for indexing
            TArray<uint32> SpellArray = KnownSpells.Array();
            SpellArray.Sort();

            if (Index - 1 < SpellArray.Num())
            {
                uint32 SpellId = SpellArray[Index - 1];
                lua_pushstring(L, "SPELL"); // spellType
                lua_pushnumber(L, SpellId); // spellId
                return 2;
            }
        }
    }

    lua_pushstring(L, "SPELL");
    lua_pushnumber(L, 0);
    return 2;
}

// ─── Item stubs ─────────────────────────────────────────────────────────────────
static int L_GetItemInfo(lua_State* L)
{
    // GetItemInfo(itemId) → name, link, quality, iLevel, reqLevel, class, subclass, maxStack, equipSlot, texture, vendorPrice
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    uint32 ItemId = static_cast<uint32>(luaL_checknumber(L, 1));

    // For now, check if we have cached item data from inventory/server
    if (Ctx && Ctx->ConnectionManager)
    {
        // TODO: Check if item data is cached from server packets or inventory
        // For now, return improved defaults based on common items

        // Check for well-known item ranges
        if (ItemId >= 6948 && ItemId <= 6948) // Hearthstone
        {
            lua_pushstring(L, "Hearthstone");
            lua_pushnil(L);
            lua_pushnumber(L, 1); // Poor quality (white)
            lua_pushnumber(L, 1);
            lua_pushnumber(L, 1);
            lua_pushstring(L, "Miscellaneous");
            lua_pushstring(L, "");
            lua_pushnumber(L, 1);
            lua_pushstring(L, "");
            lua_pushstring(L, "Interface\\Icons\\INV_Misc_Rune_01");
            lua_pushnumber(L, 0);
            return 11;
        }
        else if (ItemId >= 2447 && ItemId <= 2447) // Peacebloom
        {
            lua_pushstring(L, "Peacebloom");
            lua_pushnil(L);
            lua_pushnumber(L, 1); // Poor quality
            lua_pushnumber(L, 1);
            lua_pushnumber(L, 1);
            lua_pushstring(L, "Trade Goods");
            lua_pushstring(L, "Herb");
            lua_pushnumber(L, 20);
            lua_pushstring(L, "");
            lua_pushstring(L, "Interface\\Icons\\INV_Misc_Flower_02");
            lua_pushnumber(L, 125);
            return 11;
        }
    }

    // Default fallback for unknown items
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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 BagId = static_cast<int32>(luaL_checknumber(L, 1));
    int32 SlotId = static_cast<int32>(luaL_checknumber(L, 2));

    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint64 ItemGuid = 0;

            if (BagId == 0) // Backpack
            {
                ItemGuid = Player->GetBackpackItemGuid(static_cast<uint8>(SlotId - 1)); // Convert 1-based to 0-based
            }
            else if (BagId >= 1 && BagId <= 4) // Additional bags
            {
                uint64 BagGuid = Player->GetBagGuid(static_cast<uint8>(BagId));
                if (BagGuid != 0)
                {
                    FWowContainerEntity* Container = Ctx->EntityManager->FindContainer(BagGuid);
                    if (Container)
                    {
                        ItemGuid = Container->GetItemGuidAtSlot(SlotId - 1); // Convert 1-based to 0-based
                    }
                }
            }

            if (ItemGuid != 0)
            {
                FWowItemEntity* Item = Ctx->EntityManager->FindItem(ItemGuid);
                if (Item)
                {
                    FString IconPath = FString::Printf(TEXT("Interface\\Icons\\INV_Misc_Item_%d"), Item->Entry);
                    lua_pushstring(L, TCHAR_TO_UTF8(*IconPath)); // texture
                    lua_pushnumber(L, Item->GetStackCount()); // count
                    lua_pushboolean(L, 0); // locked
                    lua_pushnumber(L, 1); // quality (would need item DBC lookup)
                    lua_pushboolean(L, 0); // readable
                    lua_pushboolean(L, 0); // lootable

                    // Create basic item link
                    FString ItemLink = FString::Printf(TEXT("|cffffffff|Hitem:%d:0:0:0:0:0:0:0:0:0|h[Item %d]|h|r"), Item->Entry, Item->Entry);
                    lua_pushstring(L, TCHAR_TO_UTF8(*ItemLink)); // link
                    return 7;
                }
            }
        }
    }

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
    int32 BagId = static_cast<int32>(luaL_checknumber(L, 1));

    if (BagId == 0) // Backpack
    {
        lua_pushnumber(L, 16); // Standard backpack size
        return 1;
    }

    // For other bags, check if they exist and get their slot count
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player && BagId >= 1 && BagId <= 4)
        {
            uint64 BagGuid = Player->GetBagGuid(static_cast<uint8>(BagId));
            if (BagGuid != 0)
            {
                FWowContainerEntity* Container = Ctx->EntityManager->FindContainer(BagGuid);
                if (Container)
                {
                    lua_pushnumber(L, Container->GetNumSlots());
                    return 1;
                }
            }
        }
    }

    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetContainerItemLink(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 BagId = static_cast<int32>(luaL_checknumber(L, 1));
    int32 SlotId = static_cast<int32>(luaL_checknumber(L, 2));

    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint64 ItemGuid = 0;

            if (BagId == 0) // Backpack
            {
                ItemGuid = Player->GetBackpackItemGuid(static_cast<uint8>(SlotId - 1)); // Convert 1-based to 0-based
            }
            else if (BagId >= 1 && BagId <= 4) // Additional bags
            {
                uint64 BagGuid = Player->GetBagGuid(static_cast<uint8>(BagId));
                if (BagGuid != 0)
                {
                    FWowContainerEntity* Container = Ctx->EntityManager->FindContainer(BagGuid);
                    if (Container)
                    {
                        ItemGuid = Container->GetItemGuidAtSlot(SlotId - 1); // Convert 1-based to 0-based
                    }
                }
            }

            if (ItemGuid != 0)
            {
                FWowItemEntity* Item = Ctx->EntityManager->FindItem(ItemGuid);
                if (Item)
                {
                    // Create basic item link
                    FString ItemLink = FString::Printf(TEXT("|cffffffff|Hitem:%d:0:0:0:0:0:0:0:0:0|h[Item %d]|h|r"), Item->Entry, Item->Entry);
                    lua_pushstring(L, TCHAR_TO_UTF8(*ItemLink));
                    return 1;
                }
            }
        }
    }

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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 SlotId = static_cast<int32>(luaL_checknumber(L, 1));

    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player && SlotId >= 0 && SlotId < 19) // 0-18 are equipment slots
        {
            uint64 ItemGuid = Player->GetEquipmentItemGuid(static_cast<uint8>(SlotId));
            if (ItemGuid != 0)
            {
                FWowItemEntity* Item = Ctx->EntityManager->FindItem(ItemGuid);
                if (Item)
                {
                    // TODO: Look up item texture from ItemDisplayInfo DBC
                    // For now, return a generic texture based on slot
                    const char* SlotTextures[] = {
                        "Interface\\Icons\\INV_Helmet_02", // Head
                        "Interface\\Icons\\INV_Jewelry_Necklace_03", // Neck
                        "Interface\\Icons\\INV_Shoulder_05", // Shoulder
                        "Interface\\Icons\\INV_Shirt_02", // Shirt
                        "Interface\\Icons\\INV_Chest_Leather_01", // Chest
                        "Interface\\Icons\\INV_Belt_05", // Waist
                        "Interface\\Icons\\INV_Pants_03", // Legs
                        "Interface\\Icons\\INV_Boots_08", // Feet
                        "Interface\\Icons\\INV_Bracer_03", // Wrist
                        "Interface\\Icons\\INV_Gauntlets_05", // Hands
                        "Interface\\Icons\\INV_Jewelry_Ring_03", // Finger1
                        "Interface\\Icons\\INV_Jewelry_Ring_02", // Finger2
                        "Interface\\Icons\\INV_Jewelry_Talisman_01", // Trinket1
                        "Interface\\Icons\\INV_Jewelry_Talisman_02", // Trinket2
                        "Interface\\Icons\\INV_Misc_Cape_01", // Back
                        "Interface\\Icons\\INV_Weapon_Rifle_01", // Main Hand
                        "Interface\\Icons\\INV_Shield_05", // Off Hand
                        "Interface\\Icons\\INV_Weapon_Bow_01", // Ranged
                        "Interface\\Icons\\INV_Misc_Tabard_01" // Tabard
                    };

                    if (SlotId < UE_ARRAY_COUNT(SlotTextures))
                    {
                        lua_pushstring(L, SlotTextures[SlotId]);
                        return 1;
                    }
                }
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

// L_GetInventorySlotInfo defined before RegisterStubs

static int L_GetItemCount(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 ItemId = static_cast<int32>(luaL_checknumber(L, 1));

    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            int32 TotalCount = 0;

            // Check backpack
            for (uint8 i = 0; i < 16; ++i)
            {
                uint64 ItemGuid = Player->GetBackpackItemGuid(i);
                if (ItemGuid != 0)
                {
                    FWowItemEntity* Item = Ctx->EntityManager->FindItem(ItemGuid);
                    if (Item && Item->Entry == static_cast<uint32>(ItemId))
                    {
                        TotalCount += Item->GetStackCount();
                    }
                }
            }

            // Check additional bags
            for (uint8 BagIndex = 1; BagIndex <= 4; ++BagIndex)
            {
                uint64 BagGuid = Player->GetBagGuid(BagIndex);
                if (BagGuid != 0)
                {
                    FWowContainerEntity* Container = Ctx->EntityManager->FindContainer(BagGuid);
                    if (Container)
                    {
                        int32 NumSlots = Container->GetNumSlots();
                        for (int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex)
                        {
                            uint64 ItemGuid = Container->GetItemGuidAtSlot(SlotIndex);
                            if (ItemGuid != 0)
                            {
                                FWowItemEntity* Item = Ctx->EntityManager->FindItem(ItemGuid);
                                if (Item && Item->Entry == static_cast<uint32>(ItemId))
                                {
                                    TotalCount += Item->GetStackCount();
                                }
                            }
                        }
                    }
                }
            }

            lua_pushnumber(L, TotalCount);
            return 1;
        }
    }

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
    // WoW 3.3.5: GetNumFriends() → numTotal, numOnline
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const TArray<FWowFriendInfo>& FriendsList = Ctx->ConnectionManager->PacketHandler.FriendsList;
        int32 Total = FriendsList.Num();
        int32 Online = 0;
        for (const FWowFriendInfo& F : FriendsList)
        {
            if (F.Status > 0) Online++;
        }
        lua_pushnumber(L, Total);
        lua_pushnumber(L, Online);
        return 2;
    }

    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    return 2;
}

static int L_GetFriendInfo(lua_State* L)
{
    // GetFriendInfo(index) → name, level, class, area, online, status, note
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Index = static_cast<int32>(luaL_checknumber(L, 1)) - 1; // Convert 1-based to 0-based

    if (Ctx && Ctx->ConnectionManager && Index >= 0)
    {
        const TArray<FWowFriendInfo>& FriendsList = Ctx->ConnectionManager->PacketHandler.FriendsList;
        if (Index < FriendsList.Num())
        {
            const FWowFriendInfo& Friend = FriendsList[Index];

            lua_pushstring(L, TCHAR_TO_UTF8(*Friend.Name)); // name
            lua_pushnumber(L, Friend.Level); // level

            // Convert class ID to class name
            const char* ClassName = "Unknown";
            if (Friend.Class >= 1 && Friend.Class <= 11)
            {
                ClassName = WowClassNames[Friend.Class];
            }
            lua_pushstring(L, ClassName); // class

            // TODO: Look up area name from AreaTable DBC
            lua_pushstring(L, "Unknown Area"); // area

            lua_pushboolean(L, Friend.Status > 0 ? 1 : 0); // online

            // Convert status to string
            const char* StatusStr = "Offline";
            switch (Friend.Status)
            {
            case 1: StatusStr = "Online"; break;
            case 2: StatusStr = "Away"; break;
            case 3: StatusStr = "Do Not Disturb"; break;
            }
            lua_pushstring(L, StatusStr); // status

            lua_pushstring(L, TCHAR_TO_UTF8(*Friend.Note)); // note
            return 7;
        }
    }

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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const TArray<FWowGuildMember>& GuildRoster = Ctx->ConnectionManager->PacketHandler.GuildRoster;
        lua_pushnumber(L, GuildRoster.Num());
        return 1;
    }

    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetGuildRosterInfo(lua_State* L)
{
    // GetGuildRosterInfo(index) → name, rank, rankIndex, level, class, zone, note, officernote, online, status, classFileName
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 Index = static_cast<int32>(luaL_checknumber(L, 1)) - 1; // Convert 1-based to 0-based

    if (Ctx && Ctx->ConnectionManager && Index >= 0)
    {
        const TArray<FWowGuildMember>& GuildRoster = Ctx->ConnectionManager->PacketHandler.GuildRoster;
        if (Index < GuildRoster.Num())
        {
            const FWowGuildMember& Member = GuildRoster[Index];

            lua_pushstring(L, TCHAR_TO_UTF8(*Member.Name)); // name
            lua_pushstring(L, TCHAR_TO_UTF8(*Member.RankName)); // rank
            lua_pushnumber(L, Member.RankId); // rankIndex
            lua_pushnumber(L, Member.Level); // level

            // Convert class ID to class name
            const char* ClassName = "Unknown";
            const char* ClassFileName = "UNKNOWN";
            if (Member.Class >= 1 && Member.Class <= 11)
            {
                ClassName = WowClassNames[Member.Class];
                ClassFileName = WowClassTokens[Member.Class];
            }
            lua_pushstring(L, ClassName); // class

            // TODO: Look up zone name from AreaTable DBC
            lua_pushstring(L, "Unknown Zone"); // zone

            lua_pushstring(L, TCHAR_TO_UTF8(*Member.PublicNote)); // note
            lua_pushstring(L, TCHAR_TO_UTF8(*Member.OfficerNote)); // officernote
            lua_pushboolean(L, Member.Status > 0 ? 1 : 0); // online

            // Convert status to string
            const char* StatusStr = "Offline";
            switch (Member.Status)
            {
            case 1: StatusStr = "Online"; break;
            case 2: StatusStr = "Away"; break;
            case 3: StatusStr = "Do Not Disturb"; break;
            }
            lua_pushstring(L, StatusStr); // status

            lua_pushstring(L, ClassFileName); // classFileName
            return 11;
        }
    }

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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        const TArray<FWowQuestLogEntry>& QuestLog = Ctx->ConnectionManager->PacketHandler.QuestLog;
        lua_pushnumber(L, QuestLog.Num()); // numEntries
        lua_pushnumber(L, 25); // numQuests (max quest log size)
        return 2;
    }

    lua_pushnumber(L, 0); // numEntries
    lua_pushnumber(L, 25); // numQuests (max quest log size)
    return 2;
}

static int L_GetQuestLogTitle(lua_State* L)
{
    // GetQuestLogTitle(questIndex) → questTitle, level, questTag, suggestedGroup, isHeader, isCollapsed, isComplete
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 QuestIndex = static_cast<int32>(luaL_checknumber(L, 1)) - 1; // Convert 1-based to 0-based

    if (Ctx && Ctx->ConnectionManager && QuestIndex >= 0)
    {
        const TArray<FWowQuestLogEntry>& QuestLog = Ctx->ConnectionManager->PacketHandler.QuestLog;
        if (QuestIndex < QuestLog.Num())
        {
            const FWowQuestLogEntry& Entry = QuestLog[QuestIndex];

            // TODO: Look up quest details from Quest.dbc
            FString QuestTitle = FString::Printf(TEXT("Quest %d"), Entry.QuestId);
            lua_pushstring(L, TCHAR_TO_UTF8(*QuestTitle)); // questTitle
            lua_pushnumber(L, 1); // level (TODO: get from DBC)
            lua_pushstring(L, ""); // questTag
            lua_pushnumber(L, 0); // suggestedGroup
            lua_pushboolean(L, 0); // isHeader
            lua_pushboolean(L, 0); // isCollapsed
            lua_pushboolean(L, Entry.State == 1 ? 1 : 0); // isComplete
            return 7;
        }
    }

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
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    // TODO: Get selected quest log entry and return quest text
    lua_pushstring(L, ""); // questText
    return 1;
}

static int L_SelectQuestLogEntry(lua_State* L)
{
    // TODO: Store selected quest log entry for GetQuestLogQuestText
    return 0;
}

static int L_IsQuestComplete(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 QuestId = static_cast<int32>(luaL_checknumber(L, 1));

    if (Ctx && Ctx->ConnectionManager)
    {
        const TArray<FWowQuestLogEntry>& QuestLog = Ctx->ConnectionManager->PacketHandler.QuestLog;
        for (const FWowQuestLogEntry& Entry : QuestLog)
        {
            if (Entry.QuestId == static_cast<uint32>(QuestId))
            {
                lua_pushboolean(L, Entry.State == 1 ? 1 : 0);
                return 1;
            }
        }
    }

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

static FString NormalizeCVarName(const FString& Name)
{
    return Name.ToLower();
}

static FString GetDefaultCVarValue(const FString& Name)
{
    static const TMap<FString, FString> ExplicitDefaults = {
        { TEXT("chatstyle"), TEXT("classic") },
        { TEXT("conversationmode"), TEXT("inline") },
        { TEXT("showtimestamps"), TEXT("none") },
        { TEXT("locale"), TEXT("enUS") },
        { TEXT("targetoftargetmode"), TEXT("5") },
        { TEXT("displayworldpvpobjectives"), TEXT("2") },
        { TEXT("threatwarning"), TEXT("0") },
        { TEXT("combattextfloatmode"), TEXT("1") },
        { TEXT("camerasmoothstyle"), TEXT("0") },
        { TEXT("camerasmoothtrackingstyle"), TEXT("0") },
        { TEXT("enablevoicechat"), TEXT("0") },
        { TEXT("enablemicrophone"), TEXT("0") },
        { TEXT("voicechatmode"), TEXT("0") },
        { TEXT("pushtotalkbutton"), TEXT("") },
        { TEXT("pushtotalksound"), TEXT("1") },
        { TEXT("voiceactivationsensitivity"), TEXT("0.5") },
        { TEXT("sound_outputdriverindex"), TEXT("0") },
        { TEXT("sound_voicechatinputdriverindex"), TEXT("0") },
        { TEXT("sound_voicechatoutputdriverindex"), TEXT("0") },
        { TEXT("sound_numchannels"), TEXT("32") },
        { TEXT("sound_outputquality"), TEXT("1") },
        { TEXT("sound_mastervolume"), TEXT("1") },
        { TEXT("sound_musicvolume"), TEXT("1") },
        { TEXT("sound_ambiencevolume"), TEXT("1") },
        { TEXT("sound_sfxvolume"), TEXT("1") },
        { TEXT("chatsoundvolume"), TEXT("1") },
        { TEXT("chatmusicvolume"), TEXT("1") },
        { TEXT("chatambiencevolume"), TEXT("1") },
        { TEXT("inboundchatvolume"), TEXT("1") },
        { TEXT("outboundchatvolume"), TEXT("1") },
        { TEXT("sound_enableallsound"), TEXT("1") },
        { TEXT("sound_enablemusic"), TEXT("1") },
        { TEXT("sound_enableambience"), TEXT("1") },
        { TEXT("sound_enablesfx"), TEXT("1") },
        { TEXT("sound_enableerrorspeech"), TEXT("1") },
        { TEXT("sound_enableemotesounds"), TEXT("1") },
        { TEXT("sound_enablepetsounds"), TEXT("1") },
        { TEXT("sound_enablehardware"), TEXT("1") },
        { TEXT("sound_enablereverb"), TEXT("1") },
        { TEXT("sound_enabledspeffects"), TEXT("1") },
        { TEXT("sound_enablesoftwarehrtf"), TEXT("0") },
        { TEXT("sound_enablesoundwhengameisinbg"), TEXT("0") },
        { TEXT("sound_listeneratcharacter"), TEXT("0") },
        { TEXT("sound_zonemusicnodelay"), TEXT("0") },
    };

    const FString Normalized = NormalizeCVarName(Name);
    if (const FString* Explicit = ExplicitDefaults.Find(Normalized))
    {
        return *Explicit;
    }

    if (Normalized.Contains(TEXT("driverindex")))
    {
        return TEXT("0");
    }

    if (Normalized.Contains(TEXT("volume")))
    {
        return TEXT("1");
    }

    if (Normalized.Contains(TEXT("style")) || Normalized.Contains(TEXT("mode")))
    {
        return TEXT("0");
    }

    if (Normalized.StartsWith(TEXT("sound_enable")))
    {
        return TEXT("1");
    }

    if (Normalized.StartsWith(TEXT("show"))
        || Normalized.StartsWith(TEXT("display"))
        || Normalized.StartsWith(TEXT("auto"))
        || Normalized.StartsWith(TEXT("enable"))
        || Normalized.StartsWith(TEXT("nameplate"))
        || Normalized.StartsWith(TEXT("unitname"))
        || Normalized.StartsWith(TEXT("fct"))
        || Normalized.StartsWith(TEXT("camera"))
        || Normalized.StartsWith(TEXT("chat"))
        || Normalized.StartsWith(TEXT("threat"))
        || Normalized.StartsWith(TEXT("combat"))
        || Normalized.StartsWith(TEXT("mouse"))
        || Normalized.StartsWith(TEXT("movie"))
        || Normalized.StartsWith(TEXT("loot"))
        || Normalized.StartsWith(TEXT("guild"))
        || Normalized.StartsWith(TEXT("showtoast")))
    {
        return TEXT("0");
    }

    return TEXT("0");
}

static FString GetCurrentOrDefaultCVarValue(const FString& Name)
{
    const FString Normalized = NormalizeCVarName(Name);
    if (const FString* Value = CVarStorage.Find(Normalized))
    {
        return *Value;
    }

    return GetDefaultCVarValue(Name);
}

static int L_SetCVar(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    CVarStorage.Add(NormalizeCVarName(UTF8_TO_TCHAR(name)), UTF8_TO_TCHAR(value));
    return 0;
}

static int L_GetCVar(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const FString Value = GetCurrentOrDefaultCVarValue(UTF8_TO_TCHAR(name));
    FTCHARToUTF8 Conv(*Value);
    lua_pushstring(L, Conv.Get());
    return 1;
}

static int L_GetCVarBool(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const FString Value = GetCurrentOrDefaultCVarValue(UTF8_TO_TCHAR(name));
    lua_pushboolean(L, Value.Equals(TEXT("1")) || Value.ToBool() ? 1 : 0);
    return 1;
}

static int L_GetCVarDefault(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const FString Value = GetDefaultCVarValue(UTF8_TO_TCHAR(name));
    FTCHARToUTF8 Conv(*Value);
    lua_pushstring(L, Conv.Get());
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
    // WoW PlaySound accepts both number (sound ID) and string (sound name)
    if (lua_isnumber(L, 1))
    {
        UE_LOG(LogWowLuaStub, Verbose, TEXT("PlaySound(%d)"), (int)lua_tonumber(L, 1));
    }
    else if (lua_isstring(L, 1))
    {
        UE_LOG(LogWowLuaStub, Verbose, TEXT("PlaySound('%s')"), UTF8_TO_TCHAR(lua_tostring(L, 1)));
    }
    return 0;
}

static int L_PlaySoundFile(lua_State* L)
{
    const char* SoundPath = luaL_checkstring(L, 1);

    // TODO: Integrate with WowAudioManager when available through game context
    // For now, just log the sound request for debugging
    UE_LOG(LogWowLuaStub, Log, TEXT("PlaySoundFile(%s) called"), UTF8_TO_TCHAR(SoundPath));
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
    luaL_checktype(L, 1, LUA_TTABLE); // texture widget
    const char* Unit = luaL_checkstring(L, 2);

    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->FrameManager) return 0;

    // Get the entity for the unit
    FWowEntity* Entity = ResolveUnit(L, 2);
    if (!Entity || !Entity->IsUnit()) return 0;

    FWowUnitEntity* UnitEntity = static_cast<FWowUnitEntity*>(Entity);

    // Get race and gender for portrait texture
    uint8 RaceId = UnitEntity->GetRaceId();
    uint8 Gender = UnitEntity->GetGenderId(); // 0 = male, 1 = female

    // Map race IDs to portrait names (WoW 3.3.5a races)
    const char* RaceNames[] = {
        "", "Human", "Orc", "Dwarf", "NightElf", "Undead", "Tauren", "Gnome", "Troll", "", "BloodElf", "Draenei"
    };

    const char* GenderNames[] = { "Male", "Female" };

    FString PortraitPath;
    if (RaceId > 0 && RaceId < UE_ARRAY_COUNT(RaceNames) && RaceNames[RaceId] && *RaceNames[RaceId])
    {
        // Use temporary portraits: Interface\CharacterFrame\TEMPORARYPORTRAIT-Female-BloodElf.tga
        PortraitPath = FString::Printf(TEXT("Interface\\CharacterFrame\\TEMPORARYPORTRAIT-%s-%s"),
            UTF8_TO_TCHAR(GenderNames[Gender]), UTF8_TO_TCHAR(RaceNames[RaceId]));
    }
    else
    {
        // Fallback to generic portrait
        PortraitPath = TEXT("Interface\\CharacterFrame\\TEMPORARYPORTRAIT-Male-Human");
    }

    // Get the texture name from the widget
    lua_getfield(L, 1, "__name");
    const char* TextureName = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
    lua_pop(L, 1);

    if (TextureName)
    {
        UImage* NamedImage = Ctx->FrameManager->GetNamedTexture(UTF8_TO_TCHAR(TextureName));
        UTexture2D* Texture = Ctx->FrameManager->LoadUITexture(*PortraitPath);

        if (Texture && NamedImage)
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(Texture);
            Brush.DrawAs = ESlateBrushDrawType::Image;
            Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
            Brush.Tiling = ESlateBrushTileType::NoTile;
            NamedImage->SetBrush(Brush);
        }
    }

    // Store the texture path in the widget table
    lua_pushstring(L, TCHAR_TO_UTF8(*PortraitPath));
    lua_setfield(L, 1, "__texture");

    return 0;
}

static int L_SetPortraitToTexture(lua_State* L)
{
    // SetPortraitToTexture(texture, texturePath)
    luaL_checktype(L, 1, LUA_TTABLE); // texture widget
    const char* TexturePath = luaL_checkstring(L, 2);

    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->FrameManager) return 0;

    // Get the texture name from the widget
    lua_getfield(L, 1, "__name");
    const char* TextureName = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
    lua_pop(L, 1);

    if (TextureName)
    {
        UImage* NamedImage = Ctx->FrameManager->GetNamedTexture(UTF8_TO_TCHAR(TextureName));
        UTexture2D* Texture = Ctx->FrameManager->LoadUITexture(UTF8_TO_TCHAR(TexturePath));

        if (Texture && NamedImage)
        {
            FSlateBrush Brush;
            Brush.SetResourceObject(Texture);
            Brush.DrawAs = ESlateBrushDrawType::Image;
            Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
            Brush.Tiling = ESlateBrushTileType::NoTile;
            NamedImage->SetBrush(Brush);
        }
    }

    // Store the texture path in the widget table
    lua_pushstring(L, TexturePath);
    lua_setfield(L, 1, "__texture");

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
static TArray<FWowTocData> LoadedAddons;
static TArray<FString> LoadedAddonNamesByIndex;
static TSet<FString> RuntimeLoadedAddons;
static FMpqManager* AddonLoaderMpq = nullptr;
static FWowLuaVM* AddonLoaderLuaVM = nullptr;
static FWowFrameManager* AddonLoaderFrameManager = nullptr;
static FWowEventSystem* AddonLoaderEventSystem = nullptr;

// Helper function to update the loaded addons list
namespace WowLuaApi
{
    void SetAddonLoaderContext(FMpqManager* Mpq, FWowLuaVM* LuaVM, FWowFrameManager* FrameManager, FWowEventSystem* EventSystem)
    {
        AddonLoaderMpq = Mpq;
        AddonLoaderLuaVM = LuaVM;
        AddonLoaderFrameManager = FrameManager;
        AddonLoaderEventSystem = EventSystem;
        RuntimeLoadedAddons.Empty();
        UpdateLoadedAddons(Mpq);
    }

    void MarkAddonLoaded(const FString& AddonName)
    {
        RuntimeLoadedAddons.Add(AddonName.ToLower());
    }

    void UpdateLoadedAddons(FMpqManager* Mpq)
    {
        if (!Mpq) return;

        LoadedAddons.Empty();
        LoadedAddonNamesByIndex.Empty();
        TArray<FString> AddonNames = FWowAddonLoader::DiscoverAddons(Mpq);

        for (const FString& AddonName : AddonNames)
        {
            FString TocPath = FString::Printf(TEXT("Interface\\AddOns\\%s\\%s.toc"), *AddonName, *AddonName);
            TArray<uint8> TocData;
            if (Mpq->ReadFile(TocPath, TocData))
            {
                FWowTocData TocInfo = FWowAddonLoader::ParseToc(TocData);
                if (TocInfo.Title.IsEmpty())
                    TocInfo.Title = AddonName;
                LoadedAddons.Add(TocInfo);
                LoadedAddonNamesByIndex.Add(AddonName);
            }
        }
    }
}

static int L_GetNumAddOns(lua_State* L)
{
    // For now, return the number of loaded addons
    // In a full implementation, this would scan all available addons
    lua_pushnumber(L, LoadedAddons.Num());
    return 1;
}

static int L_GetAddOnInfo(lua_State* L)
{
    // GetAddOnInfo(index) → name, title, notes, loadable, reason, security, newVersion
    int32 Index = static_cast<int32>(luaL_checknumber(L, 1)) - 1; // Convert 1-based to 0-based

    if (Index >= 0 && Index < LoadedAddons.Num() && Index < LoadedAddonNamesByIndex.Num())
    {
        const FWowTocData& Addon = LoadedAddons[Index];
        const FString& AddonName = LoadedAddonNamesByIndex[Index];

        lua_pushstring(L, TCHAR_TO_UTF8(*AddonName)); // name
        lua_pushstring(L, TCHAR_TO_UTF8(*Addon.Title)); // title
        lua_pushstring(L, TCHAR_TO_UTF8(*Addon.Notes)); // notes
        lua_pushboolean(L, !Addon.bDisabled ? 1 : 0); // loadable
        lua_pushstring(L, Addon.bDisabled ? "DISABLED" : ""); // reason
        lua_pushstring(L, "SECURE"); // security
        lua_pushboolean(L, 0); // newVersion
        return 7;
    }

    lua_pushnil(L); // name
    lua_pushnil(L); // title
    lua_pushnil(L); // notes
    lua_pushboolean(L, 0); // loadable
    lua_pushstring(L, "NOT_FOUND"); // reason
    lua_pushstring(L, "SECURE"); // security
    lua_pushboolean(L, 0); // newVersion
    return 7;
}

static int L_IsAddOnLoaded(lua_State* L)
{
    const char* AddonName = luaL_checkstring(L, 1);
    FString AddonNameStr = UTF8_TO_TCHAR(AddonName);

    lua_pushboolean(L, RuntimeLoadedAddons.Contains(AddonNameStr.ToLower()) ? 1 : 0);
    return 1;
}
STUB_RETURN_NONE(EnableAddOn)
STUB_RETURN_NONE(DisableAddOn)
static int L_LoadAddOn(lua_State* L)
{
    FString AddonName;
    if (lua_isnumber(L, 1))
    {
        int32 Index = static_cast<int32>(lua_tonumber(L, 1)) - 1;
        if (Index >= 0 && Index < LoadedAddonNamesByIndex.Num())
        {
            AddonName = LoadedAddonNamesByIndex[Index];
        }
    }
    else
    {
        AddonName = UTF8_TO_TCHAR(luaL_checkstring(L, 1));
    }

    if (AddonName.IsEmpty())
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "NOT_FOUND");
        return 2;
    }

    if (RuntimeLoadedAddons.Contains(AddonName.ToLower()))
    {
        lua_pushboolean(L, 1);
        lua_pushnil(L);
        return 2;
    }

    if (!AddonLoaderMpq || !AddonLoaderLuaVM)
    {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "MISSING_CONTEXT");
        return 2;
    }

    const bool bLoaded = FWowAddonLoader::LoadAddon(
        AddonName,
        AddonLoaderMpq,
        AddonLoaderLuaVM,
        AddonLoaderFrameManager,
        AddonLoaderEventSystem);

    lua_pushboolean(L, bLoaded ? 1 : 0);
    if (bLoaded)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, "FAILED");
    }
    return 2;
}
static int L_GetNumBindings(lua_State* L)
{
    // Return basic number of hardcoded bindings
    lua_pushnumber(L, 10);
    return 1;
}

static int L_GetBinding(lua_State* L)
{
    // GetBinding(index) → category, action
    int32 Index = static_cast<int32>(luaL_checknumber(L, 1));

    // Hardcoded basic bindings
    const char* Categories[] = { "MOVEMENT", "ACTIONBAR", "TARGETING", "CAMERA", "COMBAT" };
    const char* Actions[] = { "MOVEFORWARD", "ACTIONBUTTON1", "TARGETNEARESTENEMY", "CAMERAZOOMIN", "STARTATTACK" };

    if (Index >= 1 && Index <= 5)
    {
        lua_pushstring(L, Categories[Index - 1]); // category
        lua_pushstring(L, Actions[Index - 1]); // action
        return 2;
    }

    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

static int L_GetBindingKey(lua_State* L)
{
    const char* Action = luaL_checkstring(L, 1);

    // Hardcoded key bindings for common actions
    if (strcmp(Action, "MOVEFORWARD") == 0)
    {
        lua_pushstring(L, "W");
    }
    else if (strcmp(Action, "ACTIONBUTTON1") == 0)
    {
        lua_pushstring(L, "1");
    }
    else if (strcmp(Action, "TARGETNEARESTENEMY") == 0)
    {
        lua_pushstring(L, "TAB");
    }
    else if (strcmp(Action, "STARTATTACK") == 0)
    {
        lua_pushstring(L, "SPACE");
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}
STUB_RETURN_NIL(GetBindingAction)
STUB_RETURN_FALSE(RunBinding)
STUB_RETURN_NONE(SetBinding)
STUB_RETURN_NONE(SaveBindings)

// ─── Shapeshift Forms ──────────────────────────────────────────────────────────
static int L_GetNumShapeshiftForms(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint8 ClassId = Player->GetClassId();
            // Druid (class 11) has forms, others typically don't
            if (ClassId == 11) // Druid
            {
                lua_pushnumber(L, 4); // Bear, Aquatic, Cat, Travel forms
                return 1;
            }
        }
    }
    lua_pushnumber(L, 0);
    return 1;
}

static int L_GetShapeshiftFormInfo(lua_State* L)
{
    // GetShapeshiftFormInfo(index) → texture, name, isActive, isCastable
    int32 Index = static_cast<int32>(luaL_checknumber(L, 1));

    // Hardcoded druid forms for now
    const char* FormTextures[] = { "", "Interface\\Icons\\Ability_Racial_BearForm",
                                   "Interface\\Icons\\Ability_Druid_AquaticForm",
                                   "Interface\\Icons\\Ability_Druid_CatForm",
                                   "Interface\\Icons\\Ability_Druid_TravelForm" };
    const char* FormNames[] = { "", "Bear Form", "Aquatic Form", "Cat Form", "Travel Form" };

    if (Index >= 1 && Index <= 4)
    {
        lua_pushstring(L, FormTextures[Index]); // texture
        lua_pushstring(L, FormNames[Index]); // name
        lua_pushboolean(L, 0); // isActive (TODO: check current form)
        lua_pushboolean(L, 1); // isCastable
        return 4;
    }

    lua_pushnil(L); // texture
    lua_pushstring(L, ""); // name
    lua_pushboolean(L, 0); // isActive
    lua_pushboolean(L, 0); // isCastable
    return 4;
}

// ─── Map Functions ──────────────────────────────────────────────────────────────
static int L_GetRealZoneText(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->ConnectionManager)
    {
        // Try to get zone from connection manager if available
        // For now, return default since we don't have zone tracking implemented
        lua_pushstring(L, "Stormwind City"); // Updated default zone
        return 1;
    }
    lua_pushstring(L, "Unknown Zone");
    return 1;
}

static int L_GetSubZoneText(lua_State* L)
{
    // Return the same as zone text for now as suggested
    return L_GetRealZoneText(L);
}

static int L_GetMinimapZoneText(lua_State* L)
{
    // Usually same as zone text
    return L_GetRealZoneText(L);
}

static int L_GetZonePVPInfo(lua_State* L)
{
    // GetZonePVPInfo() → pvpType, isFFA, faction
    lua_pushstring(L, "friendly"); // pvpType
    lua_pushboolean(L, 0); // isFFA
    lua_pushstring(L, "Alliance"); // faction
    return 3;
}

// ─── Talent Functions ───────────────────────────────────────────────────────────
static int L_GetNumTalentTabs(lua_State* L)
{
    // Most classes have 3 talent trees
    lua_pushnumber(L, 3);
    return 1;
}

static int L_GetTalentTabInfo(lua_State* L)
{
    // GetTalentTabInfo(tab) → name, icon, pointsSpent, fileName
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 TabIndex = static_cast<int32>(luaL_checknumber(L, 1));

    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            uint8 ClassId = Player->GetClassId();

            // Get talent tabs for this class from TalentTab DBC
            const FTalentTabDbc& TalentTabs = FDbcStore::Get().TalentTabs();
            int32 TabCount = 0;
            for (const FTalentTabDbcEntry& TabEntry : TalentTabs.GetAll())
            {
                uint32 ClassMask = 1u << (ClassId - 1);
                if ((TabEntry.ClassMask & ClassMask) != 0)
                {
                    if (TabCount == TabIndex - 1) // Convert 1-based to 0-based
                    {
                        lua_pushstring(L, TCHAR_TO_UTF8(*TabEntry.Name)); // name
                        FString IconPath = FString::Printf(TEXT("Interface\\Icons\\Spell_Icon_%d"), TabEntry.SpellIcon);
                        lua_pushstring(L, TCHAR_TO_UTF8(*IconPath)); // icon
                        lua_pushnumber(L, 0); // pointsSpent (TODO: calculate from talents)
                        lua_pushstring(L, TCHAR_TO_UTF8(*TabEntry.InternalName)); // fileName
                        return 4;
                    }
                    TabCount++;
                }
            }
        }
    }

    // Fallback
    const char* TabNames[] = { "Tree 1", "Tree 2", "Tree 3" };
    if (TabIndex >= 1 && TabIndex <= 3)
    {
        lua_pushstring(L, TabNames[TabIndex - 1]); // name
        lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark"); // icon
        lua_pushnumber(L, 0); // pointsSpent
        lua_pushstring(L, TabNames[TabIndex - 1]); // fileName
        return 4;
    }

    lua_pushstring(L, ""); // name
    lua_pushstring(L, ""); // icon
    lua_pushnumber(L, 0); // pointsSpent
    lua_pushstring(L, ""); // fileName
    return 4;
}

static int L_GetNumTalents(lua_State* L)
{
    // GetNumTalents(tab) → numTalents
    // Each talent tree typically has around 30-35 talents
    lua_pushnumber(L, 30);
    return 1;
}

static int L_GetTalentInfo(lua_State* L)
{
    // GetTalentInfo(tab, index) → name, icon, tier, column, rank, maxRank, isExceptional, meetsPrereq
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    int32 TabIndex = static_cast<int32>(luaL_checknumber(L, 1));
    int32 TalentIndex = static_cast<int32>(luaL_checknumber(L, 2));

    if (Ctx && Ctx->ConnectionManager)
    {
        // TODO: Look up talent from Talent DBC and player's talent data
        const TArray<FWowTalentInfo>& PlayerTalents = Ctx->ConnectionManager->PacketHandler.Talents;

        // For now, return placeholder data
        FString TalentName = FString::Printf(TEXT("Talent %d-%d"), TabIndex, TalentIndex);
        lua_pushstring(L, TCHAR_TO_UTF8(*TalentName)); // name
        lua_pushstring(L, "Interface\\Icons\\INV_Misc_QuestionMark"); // icon
        lua_pushnumber(L, (TalentIndex - 1) / 4 + 1); // tier (row)
        lua_pushnumber(L, (TalentIndex - 1) % 4 + 1); // column
        lua_pushnumber(L, 0); // rank
        lua_pushnumber(L, 5); // maxRank
        lua_pushboolean(L, 0); // isExceptional
        lua_pushboolean(L, 1); // meetsPrereq
        return 8;
    }

    lua_pushstring(L, ""); // name
    lua_pushstring(L, ""); // icon
    lua_pushnumber(L, 1); // tier
    lua_pushnumber(L, 1); // column
    lua_pushnumber(L, 0); // rank
    lua_pushnumber(L, 1); // maxRank
    lua_pushboolean(L, 0); // isExceptional
    lua_pushboolean(L, 0); // meetsPrereq
    return 8;
}

// ─── Combat Log ─────────────────────────────────────────────────────────────────
static int L_CombatLogGetCurrentEventInfo(lua_State* L)
{
    // CombatLogGetCurrentEventInfo() → timestamp, subevent, hideCaster, sourceGUID, sourceName, sourceFlags, sourceRaidFlags, destGUID, destName, destFlags, destRaidFlags, ...
    // Return basic placeholder data
    lua_pushnumber(L, FPlatformTime::Seconds()); // timestamp
    lua_pushstring(L, "SPELL_DAMAGE"); // subevent
    lua_pushboolean(L, 0); // hideCaster
    lua_pushstring(L, "0x0000000000000000"); // sourceGUID
    lua_pushstring(L, "Unknown"); // sourceName
    lua_pushnumber(L, 0); // sourceFlags
    lua_pushnumber(L, 0); // sourceRaidFlags
    lua_pushstring(L, "0x0000000000000000"); // destGUID
    lua_pushstring(L, "Unknown"); // destName
    lua_pushnumber(L, 0); // destFlags
    lua_pushnumber(L, 0); // destRaidFlags
    return 11;
}

// GetInventorySlotInfo - maps slot names to IDs and textures
static int L_GetInventorySlotInfo(lua_State* L)
{
    const char* slotName = luaL_optstring(L, 1, "");
    // Quick lookup using first char + length for common slots
    int slotId = 0;
    const char* tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Chest";

    if (strcmp(slotName, "HeadSlot") == 0) { slotId = 1; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Head"; }
    else if (strcmp(slotName, "NeckSlot") == 0) { slotId = 2; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Neck"; }
    else if (strcmp(slotName, "ShoulderSlot") == 0) { slotId = 3; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Shoulder"; }
    else if (strcmp(slotName, "ShirtSlot") == 0) { slotId = 4; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Shirt"; }
    else if (strcmp(slotName, "ChestSlot") == 0) { slotId = 5; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Chest"; }
    else if (strcmp(slotName, "WaistSlot") == 0) { slotId = 6; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Waist"; }
    else if (strcmp(slotName, "LegsSlot") == 0) { slotId = 7; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Legs"; }
    else if (strcmp(slotName, "FeetSlot") == 0) { slotId = 8; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Feet"; }
    else if (strcmp(slotName, "WristSlot") == 0) { slotId = 9; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Wrists"; }
    else if (strcmp(slotName, "HandsSlot") == 0) { slotId = 10; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Hands"; }
    else if (strcmp(slotName, "Finger0Slot") == 0) { slotId = 11; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Finger"; }
    else if (strcmp(slotName, "Finger1Slot") == 0) { slotId = 12; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Finger"; }
    else if (strcmp(slotName, "Trinket0Slot") == 0) { slotId = 13; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Trinket"; }
    else if (strcmp(slotName, "Trinket1Slot") == 0) { slotId = 14; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Trinket"; }
    else if (strcmp(slotName, "BackSlot") == 0) { slotId = 15; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Chest"; }
    else if (strcmp(slotName, "MainHandSlot") == 0) { slotId = 16; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-MainHand"; }
    else if (strcmp(slotName, "SecondaryHandSlot") == 0) { slotId = 17; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-SecondaryHand"; }
    else if (strcmp(slotName, "RangedSlot") == 0) { slotId = 18; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Ranged"; }
    else if (strcmp(slotName, "TabardSlot") == 0) { slotId = 19; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Tabard"; }
    else if (strcmp(slotName, "AmmoSlot") == 0) { slotId = 0; tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Ammo"; }
    else if (strncmp(slotName, "Bag", 3) == 0) { slotId = 20 + (slotName[3] - '0'); tex = "Interface\\PaperDoll\\UI-PaperDoll-Slot-Bag"; }

    lua_pushnumber(L, slotId);
    lua_pushstring(L, tex);
    return 2;
}

// Also fix GetClassInfo — can't use struct inside lambda due to macro expansion
static int L_GetClassInfo(lua_State* L)
{
    int classIndex = static_cast<int>(luaL_optnumber(L, 1, 1));
    static const char* classNames[] = {"Warrior","Paladin","Hunter","Rogue","Priest","Death Knight","Shaman","Mage","Warlock","Druid"};
    static const char* classFiles[] = {"WARRIOR","PALADIN","HUNTER","ROGUE","PRIEST","DEATHKNIGHT","SHAMAN","MAGE","WARLOCK","DRUID"};
    int idx = FMath::Clamp(classIndex - 1, 0, 9);
    lua_pushstring(L, classNames[idx]);
    lua_pushstring(L, classFiles[idx]);
    lua_pushnumber(L, classIndex);
    return 3;
}

static int L_UnitXP(lua_State* L)
{
    const char* UnitId = luaL_checkstring(L, 1);
    if (!UnitId || FString(UnitId) != TEXT("player"))
    {
        lua_pushnumber(L, 0);
        return 1;
    }

    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            lua_pushnumber(L, static_cast<lua_Number>(Player->GetXp()));
            return 1;
        }
    }

    lua_pushnumber(L, 0);
    return 1;
}

static int L_UnitXPMax(lua_State* L)
{
    const char* UnitId = luaL_checkstring(L, 1);
    if (!UnitId || FString(UnitId) != TEXT("player"))
    {
        lua_pushnumber(L, 1000);
        return 1;
    }

    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            lua_pushnumber(L, static_cast<lua_Number>(Player->GetNextLevelXp()));
            return 1;
        }
    }

    lua_pushnumber(L, 1000);
    return 1;
}

static int L_GetXPExhaustion(lua_State* L)
{
    // TODO: Implement rested XP when the update field is available
    // For now, return 0 (no rested XP)
    lua_pushnumber(L, 0);
    return 1;
}

static int L_BNGetNumFriends(lua_State* L)
{
    // BNGetNumFriends() → numBNetTotal, numBNetOnline, numWoWTotal, numWoWOnline
    // Return 4 values to ensure numWoWOnline is not nil
    lua_pushnumber(L, 0); // numBNetTotal
    lua_pushnumber(L, 0); // numBNetOnline
    lua_pushnumber(L, 0); // numWoWTotal
    lua_pushnumber(L, 0); // numWoWOnline
    return 4;
}

static int L_FillLocalizedClassList(lua_State* L)
{
    if (!lua_istable(L, 1)) return 0;
    const char* keys[] = {"WARRIOR","PALADIN","HUNTER","ROGUE","PRIEST","DEATHKNIGHT","SHAMAN","MAGE","WARLOCK","DRUID"};
    const char* vals[] = {"Warrior","Paladin","Hunter","Rogue","Priest","Death Knight","Shaman","Mage","Warlock","Druid"};
    for (int i = 0; i < 10; i++)
    {
        lua_pushstring(L, vals[i]);
        lua_setfield(L, 1, keys[i]);
    }
    return 0;
}

// ─── Combat Stats Functions ─────────────────────────────────────────────────────

static int L_GetCritChance(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // Try to get from combat rating first
            int32 CritRating = Player->GetCombatRating(WowCombatRating::CRIT_MELEE);
            if (CritRating > 0)
            {
                // Convert rating to percentage (approximate conversion for level 80)
                float CritPercent = CritRating / 45.91f; // 45.91 rating = 1% crit at level 80
                lua_pushnumber(L, CritPercent);
            }
            else
            {
                // Fallback: calculate from agility (approximate)
                int32 Agility = Player->GetAgility();
                float CritPercent = FMath::Max(0.0f, Agility / 52.0f + 5.0f); // Base 5% + agility bonus
                lua_pushnumber(L, CritPercent);
            }
            return 1;
        }
    }
    lua_pushnumber(L, 5.0); // Default 5% crit
    return 1;
}

static int L_GetRangedCritChance(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // Try to get from combat rating first
            int32 CritRating = Player->GetCombatRating(WowCombatRating::CRIT_RANGED);
            if (CritRating > 0)
            {
                float CritPercent = CritRating / 45.91f;
                lua_pushnumber(L, CritPercent);
            }
            else
            {
                // Fallback: calculate from agility
                int32 Agility = Player->GetAgility();
                float CritPercent = FMath::Max(0.0f, Agility / 52.0f + 5.0f);
                lua_pushnumber(L, CritPercent);
            }
            return 1;
        }
    }
    lua_pushnumber(L, 5.0); // Default 5% crit
    return 1;
}

static int L_GetSpellCritChance(lua_State* L)
{
    int32 School = static_cast<int32>(luaL_optnumber(L, 1, WowSpellSchool::PHYSICAL));

    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // Try to get from combat rating first
            int32 CritRating = Player->GetCombatRating(WowCombatRating::CRIT_SPELL);
            if (CritRating > 0)
            {
                float CritPercent = CritRating / 45.91f;
                lua_pushnumber(L, CritPercent);
            }
            else
            {
                // Fallback: calculate from intellect
                int32 Intellect = Player->GetIntellect();
                float CritPercent = FMath::Max(0.0f, Intellect / 166.67f + 1.0f); // Base 1% + intellect bonus
                lua_pushnumber(L, CritPercent);
            }
            return 1;
        }
    }
    lua_pushnumber(L, 1.0); // Default 1% spell crit
    return 1;
}

static int L_GetDodgeChance(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            int32 DodgeRating = Player->GetCombatRating(WowCombatRating::DODGE);
            if (DodgeRating > 0)
            {
                float DodgePercent = DodgeRating / 45.25f; // 45.25 rating = 1% dodge at level 80
                lua_pushnumber(L, DodgePercent);
            }
            else
            {
                // Fallback: calculate from agility
                int32 Agility = Player->GetAgility();
                float DodgePercent = FMath::Max(0.0f, Agility / 52.0f + 5.0f); // Base 5% + agility bonus
                lua_pushnumber(L, DodgePercent);
            }
            return 1;
        }
    }
    lua_pushnumber(L, 5.0); // Default 5% dodge
    return 1;
}

static int L_GetParryChance(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            int32 ParryRating = Player->GetCombatRating(WowCombatRating::PARRY);
            if (ParryRating > 0)
            {
                float ParryPercent = ParryRating / 45.25f;
                lua_pushnumber(L, ParryPercent);
            }
            else
            {
                // Base parry chance varies by class, default to 5%
                lua_pushnumber(L, 5.0);
            }
            return 1;
        }
    }
    lua_pushnumber(L, 5.0); // Default 5% parry
    return 1;
}

static int L_GetBlockChance(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // Check if player has a shield equipped in off-hand slot
            uint64 OffhandGuid = Player->GetEquipmentItemGuid(PlayerField::EQUIPMENT_SLOT_OFFHAND);
            bool HasShield = (OffhandGuid != 0); // If there's an item in off-hand, assume it could be a shield

            if (HasShield)
            {
                int32 BlockRating = Player->GetCombatRating(WowCombatRating::BLOCK);
                if (BlockRating > 0)
                {
                    float BlockPercent = BlockRating / 20.0f + 5.0f; // Base 5% + rating
                    lua_pushnumber(L, BlockPercent);
                }
                else
                {
                    lua_pushnumber(L, 5.0); // Base 5% for shield users
                }
            }
            else
            {
                lua_pushnumber(L, 0.0); // No shield = no block
            }
            return 1;
        }
    }
    lua_pushnumber(L, 0.0); // Default 0% block
    return 1;
}

static int L_GetSpellBonusDamage(lua_State* L)
{
    int32 School = static_cast<int32>(luaL_optnumber(L, 1, WowSpellSchool::PHYSICAL));

    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // TODO: Get spell power from player fields when available
            // For now, calculate from intellect
            int32 Intellect = Player->GetIntellect();
            int32 SpellPower = Intellect; // Simplified: 1 int = 1 spell power
            lua_pushnumber(L, SpellPower);
            return 1;
        }
    }
    lua_pushnumber(L, 0); // Default 0 spell power
    return 1;
}

static int L_GetSpellBonusHealing(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // TODO: Get healing power from player fields when available
            // For now, calculate from intellect and spirit
            int32 Intellect = Player->GetIntellect();
            int32 Spirit = Player->GetSpirit();
            int32 HealingPower = Intellect + Spirit / 2; // Simplified calculation
            lua_pushnumber(L, HealingPower);
            return 1;
        }
    }
    lua_pushnumber(L, 0); // Default 0 healing power
    return 1;
}

static int L_GetManaRegen(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            // Calculate mana regen from spirit (simplified)
            int32 Spirit = Player->GetSpirit();
            int32 Level = Player->GetLevel();

            // Base mana regen calculation (approximate)
            float BaseRegen = Spirit / 4.0f + Level / 2.0f;
            float CastingRegen = BaseRegen * 0.3f; // 30% while casting

            lua_pushnumber(L, BaseRegen);   // base regen
            lua_pushnumber(L, CastingRegen); // casting regen
            return 2;
        }
    }
    lua_pushnumber(L, 0); // base regen
    lua_pushnumber(L, 0); // casting regen
    return 2;
}

static int L_GetExpertise(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            int32 ExpertiseRating = Player->GetCombatRating(WowCombatRating::EXPERTISE);
            float Expertise = ExpertiseRating / 8.2f; // 8.2 rating = 1 expertise at level 80

            lua_pushnumber(L, Expertise); // main hand
            lua_pushnumber(L, Expertise); // off hand (same for now)
            return 2;
        }
    }
    lua_pushnumber(L, 0); // main hand
    lua_pushnumber(L, 0); // off hand
    return 2;
}

static int L_GetHaste(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (Ctx && Ctx->EntityManager)
    {
        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (Player)
        {
            int32 HasteRating = Player->GetCombatRating(WowCombatRating::HASTE);
            float HastePercent = HasteRating / 32.78f; // 32.78 rating = 1% haste at level 80
            lua_pushnumber(L, HastePercent);
            return 1;
        }
    }
    lua_pushnumber(L, 0.0); // Default 0% haste
    return 1;
}

static int L_ToggleSpellBook(lua_State* L)
{
    FWowLuaContext* Ctx = WowLuaApi::GetContext(L);
    if (!Ctx || !Ctx->FrameManager) return 0;

    // Find the SpellBookFrame
    int64 SpellBookHandle = Ctx->FrameManager->FindFrame(TEXT("SpellBookFrame"));
    if (SpellBookHandle < 0)
    {
        UE_LOG(LogWowLuaStub, Warning, TEXT("ToggleSpellBook: SpellBookFrame not found"));
        return 0;
    }

    // Toggle visibility: if shown, hide it; if hidden, show it
    bool bIsVisible = Ctx->FrameManager->IsFrameVisible(SpellBookHandle);
    Ctx->FrameManager->SetFrameVisible(SpellBookHandle, !bIsVisible);

    UE_LOG(LogWowLuaStub, Log, TEXT("ToggleSpellBook: %s SpellBookFrame"),
        bIsVisible ? TEXT("Hiding") : TEXT("Showing"));

    return 0;
}

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
    lua_register(L, "UnitMana", L_UnitPower);        // WoW 3.3.5 alias
    lua_register(L, "UnitManaMax", L_UnitPowerMax);   // WoW 3.3.5 alias
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
    lua_register(L, "UnitXP", L_UnitXP);
    lua_register(L, "UnitXPMax", L_UnitXPMax);
    lua_register(L, "GetXPExhaustion", L_GetXPExhaustion);

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
    lua_register(L, "PickupAction", L_PickupAction);
    lua_register(L, "PlaceAction", L_PlaceAction);
    lua_register(L, "IsCurrentAction", L_IsCurrentAction);
    lua_register(L, "IsUsableAction", L_IsUsableAction);
    lua_register(L, "IsAttackAction", L_IsAttackAction);
    lua_register(L, "IsActionInRange", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        int32 Slot = SafeActionSlot(L2);

        if (!Ctx || !Ctx->ConnectionManager || Slot < 0 || Slot >= 144)
        {
            lua_pushnil(L2); // nil = no action
            return 1;
        }

        const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
        if (Slot >= ActionButtons.Num() || ActionButtons[Slot] == 0)
        {
            lua_pushnil(L2); // nil = no action
            return 1;
        }

        // For now return in range (1) for any valid action
        // Real implementation would check spell/ability range vs target distance
        lua_pushnumber(L2, 1);
        return 1;
    });
    lua_register(L, "GetBonusBarOffset", L_GetBonusBarOffset);
    lua_register(L, "GetCurrentActionBarPage", L_GetCurrentActionBarPage);
    lua_register(L, "GetActionBarPage", L_GetCurrentActionBarPage); // WoW 3.3.5 alias
    lua_register(L, "UseAction", L_UseAction);

    // Spell
    lua_register(L, "PickupSpell", L_PickupSpell);
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

    // Shapeshift forms - temporarily disabled due to compilation issues
    // lua_register(L, "GetNumShapeshiftForms", L_GetNumShapeshiftForms);
    // lua_register(L, "GetShapeshiftFormInfo", L_GetShapeshiftFormInfo);

    // Map functions
    lua_register(L, "GetRealZoneText", L_GetRealZoneText);
    lua_register(L, "GetSubZoneText", L_GetSubZoneText);
    lua_register(L, "GetMinimapZoneText", L_GetMinimapZoneText);
    lua_register(L, "GetZonePVPInfo", L_GetZonePVPInfo);

    // Talent functions - temporarily disabled due to compilation issues
    // lua_register(L, "GetNumTalentTabs", L_GetNumTalentTabs);
    lua_register(L, "GetTalentTabInfo", L_GetTalentTabInfo);
    lua_register(L, "GetNumTalents", L_GetNumTalents);
    lua_register(L, "GetTalentInfo", L_GetTalentInfo);

    // Combat log
    lua_register(L, "CombatLogGetCurrentEventInfo", L_CombatLogGetCurrentEventInfo);

    // Set CURRENT_ACTIONBAR_PAGE global
    lua_pushnumber(L, 1);
    lua_setglobal(L, "CURRENT_ACTIONBAR_PAGE");

    // ── Critical missing stubs (from FrameXML error log) ──

    // Platform detection
    lua_register(L, "IsMacClient", [](lua_State* L2) -> int {
#if PLATFORM_MAC
        lua_pushboolean(L2, 1);
#else
        lua_pushboolean(L2, 0);
#endif
        return 1;
    });
    lua_register(L, "IsWindowsClient", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "IsLinuxClient", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });

    // CVar extended
    lua_register(L, "GetCVarDefault", L_GetCVarDefault);

    // Video/Audio options stubs
    lua_register(L, "GetCurrentMultisampleFormat", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "SetMultisampleFormat", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "IsStereoVideoAvailable", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "Sound_GameSystem_GetOutputDriverNameByIndex", [](lua_State* L2) -> int { lua_pushstring(L2, "Default"); return 1; });
    lua_register(L, "Sound_GameSystem_GetNumOutputDrivers", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "Sound_ChatSystem_GetNumOutputDrivers", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "Sound_ChatSystem_GetOutputDriverNameByIndex", [](lua_State* L2) -> int { lua_pushstring(L2, "Default"); return 1; });
    lua_register(L, "GetScreenResolutions", [](lua_State* L2) -> int { lua_pushstring(L2, "1920x1080"); return 1; });
    lua_register(L, "GetCurrentResolution", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "SetScreenResolution", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetVideoCaps", [](lua_State* L2) -> int { lua_pushstring(L2, ""); return 1; });

    // FrameXML utility functions (called from OnLoad scripts)
    lua_register(L, "ScrollFrame_OnLoad", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SmallMoneyFrame_OnLoad", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "MoneyFrame_OnLoad", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ChatFrame_OnUpdate", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ChatEdit_OnUpdate", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "MessageFrameScrollButton_OnUpdate", [](lua_State* L2) -> int { return 0; });

    // Mirror timer (breath bar etc.)
    lua_register(L, "GetMirrorTimerProgress", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetMirrorTimerInfo", [](lua_State* L2) -> int {
        lua_pushstring(L2, "UNKNOWN"); lua_pushnumber(L2, 0); lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0); lua_pushboolean(L2, 0); lua_pushstring(L2, "");
        return 6;
    });

    // Input state
    lua_register(L, "IsModifierKeyDown", [](lua_State* L2) -> int {
        if (FSlateApplication::IsInitialized())
        {
            FModifierKeysState ModKeys = FSlateApplication::Get().GetModifierKeys();
            bool AnyModDown = ModKeys.IsShiftDown() || ModKeys.IsControlDown() || ModKeys.IsAltDown();
            lua_pushboolean(L2, AnyModDown);
        }
        else
        {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsShiftKeyDown", [](lua_State* L2) -> int {
        if (FSlateApplication::IsInitialized())
        {
            lua_pushboolean(L2, FSlateApplication::Get().GetModifierKeys().IsShiftDown());
        }
        else
        {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsControlKeyDown", [](lua_State* L2) -> int {
        if (FSlateApplication::IsInitialized())
        {
            lua_pushboolean(L2, FSlateApplication::Get().GetModifierKeys().IsControlDown());
        }
        else
        {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsAltKeyDown", [](lua_State* L2) -> int {
        if (FSlateApplication::IsInitialized())
        {
            lua_pushboolean(L2, FSlateApplication::Get().GetModifierKeys().IsAltDown());
        }
        else
        {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsMouseButtonDown", [](lua_State* L2) -> int {
        if (FSlateApplication::IsInitialized())
        {
            // Check parameter for which button (1=left, 2=right, 3=middle)
            int32 Button = static_cast<int32>(luaL_optnumber(L2, 1, 1));
            TSet<FKey> PressedButtons = FSlateApplication::Get().GetPressedMouseButtons();
            bool IsPressed = false;
            if (Button == 1) IsPressed = PressedButtons.Contains(EKeys::LeftMouseButton);
            else if (Button == 2) IsPressed = PressedButtons.Contains(EKeys::RightMouseButton);
            else if (Button == 3) IsPressed = PressedButtons.Contains(EKeys::MiddleMouseButton);
            lua_pushboolean(L2, IsPressed);
        }
        else
        {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "GetMouseFocus", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetMousePosition", [](lua_State* L2) -> int {
        if (FSlateApplication::IsInitialized())
        {
            FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
            // Convert to WoW UI coordinates by dividing by UIScale (viewport_height / 768.0)
            if (GEngine && GEngine->GameViewport)
            {
                FViewport* Viewport = GEngine->GameViewport->Viewport;
                if (Viewport)
                {
                    FIntPoint ViewportSize = Viewport->GetSizeXY();
                    float UIScale = ViewportSize.Y / 768.0f;
                    float WowX = CursorPos.X / UIScale;
                    float WowY = CursorPos.Y / UIScale;
                    lua_pushnumber(L2, WowX);
                    lua_pushnumber(L2, WowY);
                    return 2;
                }
            }
            lua_pushnumber(L2, CursorPos.X);
            lua_pushnumber(L2, CursorPos.Y);
            return 2;
        }
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 2;
    });

    // Zone/Map functions
    lua_register(L, "GetRealZoneText", [](lua_State* L2) -> int { lua_pushstring(L2, "Elwynn Forest"); return 1; });
    lua_register(L, "GetSubZoneText", [](lua_State* L2) -> int { lua_pushstring(L2, ""); return 1; });
    lua_register(L, "GetMinimapZoneText", [](lua_State* L2) -> int { lua_pushstring(L2, "Elwynn Forest"); return 1; });
    lua_register(L, "GetZonePVPInfo", [](lua_State* L2) -> int { lua_pushstring(L2, "friendly"); lua_pushboolean(L2, 0); lua_pushstring(L2, ""); return 3; });

    // Shapeshift/stance
    lua_register(L, "GetNumShapeshiftForms", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (!Player)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        uint8 ClassId = Player->GetClassId();
        int32 Level = Player->GetLevel();

        switch (ClassId)
        {
        case 1: // Warrior - Battle/Defensive/Berserker stance
            if (Level >= 10) lua_pushnumber(L2, Level >= 30 ? 3 : (Level >= 20 ? 2 : 1));
            else lua_pushnumber(L2, 0);
            break;
        case 11: // Druid - Bear/Aquatic/Cat/Travel/Moonkin/Tree/Flight
            if (Level >= 15)
            {
                int32 Forms = 1; // Bear at 15
                if (Level >= 16) Forms++; // Aquatic at 16
                if (Level >= 20) Forms++; // Cat at 20
                if (Level >= 30) Forms++; // Travel at 30
                if (Level >= 40) Forms++; // Moonkin/Tree available
                if (Level >= 60) Forms++; // Flight form
                lua_pushnumber(L2, Forms);
            }
            else lua_pushnumber(L2, 0);
            break;
        case 9: // Warlock - might have metamorphosis
            lua_pushnumber(L2, Level >= 60 ? 1 : 0);
            break;
        case 6: // Death Knight - might have presence forms
            lua_pushnumber(L2, Level >= 55 ? 3 : 0);
            break;
        default:
            lua_pushnumber(L2, 0);
            break;
        }
        return 1;
    });
    lua_register(L, "GetShapeshiftFormInfo", [](lua_State* L2) -> int {
        lua_pushstring(L2, ""); lua_pushstring(L2, ""); lua_pushboolean(L2, 0); lua_pushboolean(L2, 0);
        return 4;
    });

    // Talent extended
    lua_register(L, "GetNumTalentTabs", [](lua_State* L2) -> int { lua_pushnumber(L2, 3); return 1; });
    lua_register(L, "GetActiveTalentGroup", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "GetNumTalentGroups", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "GetTalentPreviewCount", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });

    // Minimap/tracking
    lua_register(L, "GetNumTrackingTypes", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetTrackingInfo", [](lua_State* L2) -> int {
        lua_pushstring(L2, ""); lua_pushstring(L2, ""); lua_pushboolean(L2, 0); lua_pushstring(L2, "");
        return 4;
    });
    lua_register(L, "ToggleMinimap", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetPlayerFacing", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        // Get player entity
        FWowEntity* Player = Ctx->EntityManager->Find(Ctx->EntityManager->LocalPlayerGuid);
        if (!Player)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        // Return player's facing angle from movement info
        lua_pushnumber(L2, Player->Movement.Orientation);
        return 1;
    });

    // Misc commonly needed
    lua_register(L, "ToggleGameMenu", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "Logout", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "Quit", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ReloadUI", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "Screenshot", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SetUIVisibility", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "OpeningCinematic", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SetCursor", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ResetCursor", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ShowCursor", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "HideCursor", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetCursorInfo", L_GetCursorInfo);
    lua_register(L, "ClearCursor", L_ClearCursor);
    lua_register(L, "GetItemQualityColor", [](lua_State* L2) -> int {
        // Returns r,g,b,hex for item quality. Simplified: return white for all.
        lua_pushnumber(L2, 1); lua_pushnumber(L2, 1); lua_pushnumber(L2, 1);
        lua_pushstring(L2, "|cffffffff");
        return 4;
    });
    lua_register(L, "GetNumEquipmentSets", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "UnitCharacterPoints", [](lua_State* L2) -> int {
        // UnitCharacterPoints(unit) → unspentTalentPoints
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsUnit()) {
            FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
            // TODO: Get from entity data when talent system is implemented
            lua_pushnumber(L2, 0);
        } else {
            lua_pushnumber(L2, 0);
        }
        return 1;
    });

    // Chat system functions
    lua_register(L, "ChatFrame_OnLoad", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ChatEdit_OnLoad", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ChatEdit_OnUpdate", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "MessageFrameScrollButton_OnLoad", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "HybridScrollFrame_OnLoad", [](lua_State* L2) -> int { return 0; });

    // UI utility functions
    lua_register(L, "PanelTemplates_SetNumTabs", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "PanelTemplates_TabResize", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "PanelTemplates_DisableTab", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "StaticPopup_OnUpdate", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "MoneyInputFrame_SetMode", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "MoneyInputFrame_SetOnValueChangedFunc", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "MultiCastActionButton_OnLoad", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SetTextStatusBarText", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "MoneyFrame_SetType", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ScrollFrame_OnScrollRangeChanged", [](lua_State* L2) -> int { return 0; });

    // Unit functions
    lua_register(L, "UnitFactionGroup", [](lua_State* L2) -> int {
        lua_pushstring(L2, "Alliance");
        lua_pushstring(L2, "Alliance");
        return 2;
    });
    lua_register(L, "UnitIsConnected", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushboolean(L2, 0);
            return 1;
        }

        uint64 UnitGuid = ResolveUnitGuid(L2, 1);
        if (UnitGuid == 0)
        {
            lua_pushboolean(L2, 0);
            return 1;
        }

        // Return true if entity exists (means it's connected/online)
        FWowEntity* Entity = Ctx->EntityManager->Find(UnitGuid);
        lua_pushboolean(L2, Entity != nullptr);
        return 1;
    });
    lua_register(L, "UnitChannelInfo", [](lua_State* L2) -> int {
        for (int i = 0; i < 8; i++) lua_pushnil(L2);
        return 8;
    });
    lua_register(L, "UnitCreatureType", [](lua_State* L2) -> int {
        // UnitCreatureType(unit) → "Humanoid", "Beast", etc.
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsPlayer()) {
            lua_pushstring(L2, "Humanoid");
        } else if (Entity && Entity->IsUnit()) {
            // TODO: Check creature type from NPC data when available
            lua_pushstring(L2, "Humanoid");
        } else {
            lua_pushstring(L2, "Humanoid");
        }
        return 1;
    });
    lua_register(L, "UnitCreatureFamily", [](lua_State* L2) -> int {
        // UnitCreatureFamily(unit) → family name for beasts, "" for most
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsUnit()) {
            // TODO: Check creature family from NPC data for beasts
            lua_pushstring(L2, "");
        } else {
            lua_pushstring(L2, "");
        }
        return 1;
    });
    lua_register(L, "UnitClassification", [](lua_State* L2) -> int {
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (!Entity || !Entity->IsUnit())
        {
            lua_pushstring(L2, "normal");
            return 1;
        }

        FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
        uint32 UnitFlags = Unit->GetUnitFlags();
        uint32 UnitFlags2 = Unit->GetUnitFlags2();

        // Check for specific classifications based on flags
        // These are common 3.3.5a flag values
        if (UnitFlags2 & 0x8) // UNIT_FLAG2_WORLD_BOSS
        {
            lua_pushstring(L2, "worldboss");
        }
        else if (UnitFlags & 0x10) // UNIT_FLAG_RARE
        {
            if (UnitFlags & 0x2) // UNIT_FLAG_ELITE
            {
                lua_pushstring(L2, "rareelite");
            }
            else
            {
                lua_pushstring(L2, "rare");
            }
        }
        else if (UnitFlags & 0x2) // UNIT_FLAG_ELITE
        {
            lua_pushstring(L2, "elite");
        }
        else
        {
            lua_pushstring(L2, "normal");
        }
        return 1;
    });
    lua_register(L, "UnitIsPVP", [](lua_State* L2) -> int {
        // UnitIsPVP(unit) → boolean (true if flagged for PVP)
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsUnit()) {
            FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
            uint32 UnitFlags = Unit->GetUnitFlags();
            // UNIT_FLAG_PVP = 0x1000
            lua_pushboolean(L2, (UnitFlags & 0x1000) != 0);
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "UnitIsPVPFreeForAll", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "UnitPlayerControlled", [](lua_State* L2) -> int {
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (!Entity)
        {
            lua_pushboolean(L2, 0);
            return 1;
        }

        // Players are player-controlled, pets/minions controlled by players also count
        if (Entity->IsPlayer())
        {
            lua_pushboolean(L2, 1);
            return 1;
        }

        // Check if unit has a player owner/creator (for pets/minions)
        if (Entity->IsUnit())
        {
            FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
            // Check summoned by field or created by field for player control
            uint64 SummonedBy = Unit->GetField64(UnitField::SUMMONEDBY);
            uint64 CreatedBy = Unit->GetField64(UnitField::CREATEDBY);
            uint64 CharmedBy = Unit->GetField64(UnitField::CHARMEDBY);

            if (SummonedBy != 0 || CreatedBy != 0 || CharmedBy != 0)
            {
                lua_pushboolean(L2, 1);
                return 1;
            }
        }

        lua_pushboolean(L2, 0);
        return 1;
    });
    lua_register(L, "UnitIsCharmed", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "UnitIsPossessed", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "UnitCanAssist", [](lua_State* L2) -> int {
        // UnitCanAssist(unit1, unit2) → boolean (can unit1 assist unit2?)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        FWowEntity* Entity1 = ResolveUnit(L2, 1);
        FWowEntity* Entity2 = ResolveUnit(L2, 2);
        const bool bCanAssist = Entity1 && Entity2 &&
            GetUnitRelationship(Ctx, Entity1, Entity2) == EWowUnitRelationship::Friendly;
        lua_pushboolean(L2, bCanAssist ? 1 : 0);
        return 1;
    });
    lua_register(L, "UnitCanCooperate", [](lua_State* L2) -> int {
        // UnitCanCooperate(unit1, unit2) → boolean (can unit1 cooperate with unit2?)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        FWowEntity* Entity1 = ResolveUnit(L2, 1);
        FWowEntity* Entity2 = ResolveUnit(L2, 2);
        const bool bCanCooperate = Entity1 && Entity2 &&
            GetUnitRelationship(Ctx, Entity1, Entity2) == EWowUnitRelationship::Friendly;
        lua_pushboolean(L2, bCanCooperate ? 1 : 0);
        return 1;
    });
    lua_register(L, "UnitCanAttack", [](lua_State* L2) -> int {
        // UnitCanAttack(unit1, unit2) → boolean (can unit1 attack unit2?)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        FWowEntity* Entity1 = ResolveUnit(L2, 1);
        FWowEntity* Entity2 = ResolveUnit(L2, 2);
        const bool bCanAttack = Entity1 && Entity2 &&
            GetUnitRelationship(Ctx, Entity1, Entity2) == EWowUnitRelationship::Hostile;
        lua_pushboolean(L2, bCanAttack ? 1 : 0);
        return 1;
    });
    lua_register(L, "UnitInRaid", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->ConnectionManager)
        {
            lua_pushnil(L2);
            return 1;
        }

        const uint64 UnitGuid = ResolveUnitGuid(L2, 1);
        if (UnitGuid == 0)
        {
            lua_pushnil(L2);
            return 1;
        }

        const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
        if (GroupInfo.IsEmpty() || !GroupInfo.IsRaidGroup())
        {
            lua_pushnil(L2);
            return 1;
        }

        if (UnitGuid == Ctx->EntityManager->LocalPlayerGuid)
        {
            lua_pushnumber(L2, 1);
            return 1;
        }

        for (int32 MemberIndex = 0; MemberIndex < GroupInfo.Members.Num(); ++MemberIndex)
        {
            if (GroupInfo.Members[MemberIndex].Guid == UnitGuid)
            {
                lua_pushnumber(L2, MemberIndex + 2);
                return 1;
            }
        }

        lua_pushnil(L2);
        return 1;
    });
    lua_register(L, "UnitInParty", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->ConnectionManager)
        {
            lua_pushnil(L2);
            return 1;
        }

        const uint64 UnitGuid = ResolveUnitGuid(L2, 1);
        if (UnitGuid == 0)
        {
            lua_pushnil(L2);
            return 1;
        }

        const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
        if (GroupInfo.IsEmpty() || GroupInfo.IsRaidGroup())
        {
            lua_pushnil(L2);
            return 1;
        }

        if (UnitGuid == Ctx->EntityManager->LocalPlayerGuid)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        for (int32 MemberIndex = 0; MemberIndex < GroupInfo.Members.Num(); ++MemberIndex)
        {
            if (GroupInfo.Members[MemberIndex].Guid == UnitGuid)
            {
                lua_pushnumber(L2, MemberIndex + 1);
                return 1;
            }
        }

        lua_pushnil(L2);
        return 1;
    });
    lua_register(L, "UnitInVehicle", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "UnitOnTaxi", [](lua_State* L2) -> int {
        // UnitOnTaxi(unit) → boolean (true if on taxi/flight)
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsUnit()) {
            FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
            uint32 UnitFlags = Unit->GetUnitFlags();
            // UNIT_FLAG_TAXI_FLIGHT = 0x100000
            lua_pushboolean(L2, (UnitFlags & 0x100000) != 0);
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "UnitIsAFK", [](lua_State* L2) -> int {
        // UnitIsAFK(unit) → boolean (true if AFK)
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsPlayer()) {
            FWowPlayerEntity* Player = static_cast<FWowPlayerEntity*>(Entity);
            uint32 PlayerFlags = Player->GetPlayerFlags();
            // PLAYER_FLAG_AFK = 0x01
            lua_pushboolean(L2, (PlayerFlags & 0x01) != 0);
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "UnitIsDND", [](lua_State* L2) -> int {
        // UnitIsDND(unit) → boolean (true if Do Not Disturb)
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsPlayer()) {
            FWowPlayerEntity* Player = static_cast<FWowPlayerEntity*>(Entity);
            uint32 PlayerFlags = Player->GetPlayerFlags();
            // PLAYER_FLAG_DND = 0x02
            lua_pushboolean(L2, (PlayerFlags & 0x02) != 0);
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "UnitXP", L_UnitXP);
    lua_register(L, "UnitXPMax", L_UnitXPMax);
    lua_register(L, "GetXPExhaustion", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushnil(L2);
            return 1;
        }

        // Get player entity
        FWowEntity* Player = Ctx->EntityManager->Find(Ctx->EntityManager->LocalPlayerGuid);
        if (!Player || !Player->IsPlayer())
        {
            lua_pushnil(L2);
            return 1;
        }

        // Rested XP is typically stored in player fields but exact field is TBD
        // For now return 0 but maintain proper return type (number)
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetRestState", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 1);
        lua_pushstring(L2, "Normal");
        lua_pushnumber(L2, 1);
        return 3;
    });

    // Security/Addon
    lua_register(L, "issecure", [](lua_State* L2) -> int { lua_pushboolean(L2, 1); return 1; });
    lua_register(L, "issecurevariable", [](lua_State* L2) -> int {
        lua_pushboolean(L2, 1);
        lua_pushstring(L2, "");
        return 2;
    });
    lua_register(L, "BNFeaturesEnabled", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "BNConnected", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "BNFeaturesEnabledAndConnected", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });

    // Inventory/Equipment
    lua_register(L, "GetInventoryAlertStatus", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetInventoryItemDurability", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 100);
        lua_pushnumber(L2, 100);
        return 2;
    });
    lua_register(L, "InRepairMode", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "GetContainerItemCooldown", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 3;
    });
    lua_register(L, "CursorHasItem", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "CursorHasSpell", L_CursorHasSpell);
    lua_register(L, "IsEquippedAction", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });

    // Loot
    lua_register(L, "GetLootRollTimeLeft", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetLootRollItemInfo", [](lua_State* L2) -> int {
        lua_pushstring(L2, "");
        lua_pushstring(L2, "");
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 1);
        lua_pushboolean(L2, 0);
        lua_pushboolean(L2, 0);
        return 6;
    });
    lua_register(L, "GetLootRollItemLink", [](lua_State* L2) -> int { lua_pushstring(L2, ""); return 1; });

    // Misc
    lua_register(L, "SetWhoToUI", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetNumVoiceSessions", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetTrackingTexture", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetExistingLocales", [](lua_State* L2) -> int { lua_pushstring(L2, "enUS"); return 1; });
    lua_register(L, "GetCursorPosition", [](lua_State* L2) -> int {
        if (FSlateApplication::IsInitialized())
        {
            FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
            // Convert to WoW UI coordinates by dividing by UIScale (viewport_height / 768.0)
            if (GEngine && GEngine->GameViewport)
            {
                FViewport* Viewport = GEngine->GameViewport->Viewport;
                if (Viewport)
                {
                    FIntPoint ViewportSize = Viewport->GetSizeXY();
                    float UIScale = ViewportSize.Y / 768.0f;
                    lua_pushnumber(L2, CursorPos.X / UIScale);
                    lua_pushnumber(L2, CursorPos.Y / UIScale);
                    return 2;
                }
            }
            lua_pushnumber(L2, CursorPos.X);
            lua_pushnumber(L2, CursorPos.Y);
        }
        else
        {
            lua_pushnumber(L2, 0);
            lua_pushnumber(L2, 0);
        }
        return 2;
    });
    lua_register(L, "GetNumWorldStateUI", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetWatchedFactionInfo", [](lua_State* L2) -> int {
        lua_pushstring(L2, "");
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 5;
    });
    lua_register(L, "GetNumFactions", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetText", [](lua_State* L2) -> int {
        const char* key = luaL_optstring(L2, 1, "");
        lua_pushstring(L2, key);
        return 1;
    });
    lua_register(L, "HasPetUI", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushboolean(L2, 0);
            lua_pushboolean(L2, 0);
            return 2;
        }

        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (!Player)
        {
            lua_pushboolean(L2, 0);
            lua_pushboolean(L2, 0);
            return 2;
        }

        // Check if player has a pet - look for UNIT_FIELD_SUMMON
        uint64 PetGuid = Player->GetField64(UnitField::SUMMON);
        bool HasPet = (PetGuid != 0);

        // Check if it's a hunter pet (class 3 = Hunter)
        bool IsHunterPet = HasPet && (Player->GetClassId() == 3);

        lua_pushboolean(L2, HasPet);
        lua_pushboolean(L2, IsHunterPet);
        return 2;
    });
    lua_register(L, "PetHasActionBar", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "HasFullControl", [](lua_State* L2) -> int { lua_pushboolean(L2, 1); return 1; });
    lua_register(L, "GetComboPoints", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
        if (!Player)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        // Get target (combo points are typically on target)
        uint64 TargetGuid = ResolveUnitGuid(L2, 2); // second parameter is target
        if (TargetGuid == 0 && Ctx->ConnectionManager)
        {
            TargetGuid = static_cast<uint64>(Ctx->ConnectionManager->GetTargetGuid());
        }

        // Combo points are stored as part of unit fields
        // For 3.3.5a, they're typically in a power field or custom field
        // This would need the exact field mapping, for now return based on class
        uint8 ClassId = Player->GetClassId();
        if (ClassId == 4 || ClassId == 11) // Rogue or Druid
        {
            // Return 0-5 combo points - would read from actual field in real implementation
            lua_pushnumber(L2, 0);
        }
        else
        {
            lua_pushnumber(L2, 0);
        }
        return 1;
    });
    lua_register(L, "GetPVPLifetimeStats", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 3;
    });
    lua_register(L, "GetPVPSessionStats", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 2;
    });
    lua_register(L, "GetPVPYesterdayStats", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 3;
    });
    lua_register(L, "GetPVPThisWeekStats", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 3;
    });
    lua_register(L, "GetLifetimeHonorableKills", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->EntityManager)
        {
            FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
            if (Player)
            {
                // Try to read from player honor fields if available
                // Honor kills field isn't defined in current PlayerField enum, return 0 for now
                uint32 HonorKills = 0; // Player->GetField(PlayerField::LIFETIME_HONORABLE_KILLS);
                lua_pushnumber(L2, HonorKills);
                return 1;
            }
        }
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetArenaTeam", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetNumArenaTeams", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetNumArenaOpponents", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "IsInArenaTeam", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "GetDodgeChance", L_GetDodgeChance);
    lua_register(L, "GetParryChance", L_GetParryChance);
    lua_register(L, "GetBlockChance", L_GetBlockChance);
    lua_register(L, "GetShieldBlock", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        // Get player entity
        FWowEntity* Player = Ctx->EntityManager->Find(Ctx->EntityManager->LocalPlayerGuid);
        if (!Player || !Player->IsPlayer())
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        FWowPlayerEntity* PlayerEntity = static_cast<FWowPlayerEntity*>(Player);

        // Calculate shield block from block rating
        int32 BlockRating = PlayerEntity->GetCombatRating(WowCombatRating::BLOCK);

        // Simple block value calculation - this would need proper rating conversion in real game
        int32 BlockValue = BlockRating; // Simplified, real calculation involves rating-to-percentage conversion

        lua_pushnumber(L2, BlockValue);
        return 1;
    });
    lua_register(L, "GetCritChance", L_GetCritChance);
    lua_register(L, "GetRangedCritChance", L_GetRangedCritChance);
    lua_register(L, "GetSpellCritChance", L_GetSpellCritChance);
    lua_register(L, "GetCombatRating", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        int32 RatingIndex = static_cast<int32>(luaL_checknumber(L2, 1));

        if (Ctx && Ctx->EntityManager)
        {
            FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
            if (Player)
            {
                int32 Rating = Player->GetCombatRating(RatingIndex);
                lua_pushnumber(L2, Rating);
                return 1;
            }
        }
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetCombatRatingBonus", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        int32 RatingIndex = static_cast<int32>(luaL_checknumber(L2, 1));

        if (Ctx && Ctx->EntityManager)
        {
            FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
            if (Player)
            {
                int32 Rating = Player->GetCombatRating(RatingIndex);
                // Convert rating to percentage bonus using level 80 conversion rates
                float BonusPercent = 0.0f;
                switch (RatingIndex)
                {
                case WowCombatRating::CRIT_MELEE:
                case WowCombatRating::CRIT_RANGED:
                case WowCombatRating::CRIT_SPELL:
                    BonusPercent = Rating / 45.91f;
                    break;
                case WowCombatRating::HASTE:
                case WowCombatRating::HASTE_MELEE:
                case WowCombatRating::HASTE_RANGED:
                case WowCombatRating::HASTE_SPELL:
                    BonusPercent = Rating / 32.78f;
                    break;
                case WowCombatRating::HIT_MELEE:
                case WowCombatRating::HIT_RANGED:
                case WowCombatRating::HIT_SPELL:
                    BonusPercent = Rating / 32.78f;
                    break;
                case WowCombatRating::DODGE:
                case WowCombatRating::PARRY:
                    BonusPercent = Rating / 45.25f;
                    break;
                case WowCombatRating::EXPERTISE:
                    BonusPercent = Rating / 8.2f;
                    break;
                default:
                    BonusPercent = Rating / 32.78f; // Default conversion
                    break;
                }
                lua_pushnumber(L2, BonusPercent);
                return 1;
            }
        }
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetManaRegen", L_GetManaRegen);
    lua_register(L, "GetExpertise", L_GetExpertise);
    lua_register(L, "GetSpellBonusDamage", L_GetSpellBonusDamage);
    lua_register(L, "GetSpellBonusHealing", L_GetSpellBonusHealing);
    lua_register(L, "GetSpellPenetration", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->EntityManager)
        {
            FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
            if (Player)
            {
                // Spell penetration isn't a combat rating in WotLK, but could be in stats
                // For now return 0 as it's not commonly tracked
                lua_pushnumber(L2, 0);
                return 1;
            }
        }
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetMastery", [](lua_State* L2) -> int {
        // Mastery doesn't exist in WotLK (3.3.5), added in Cataclysm
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetHaste", L_GetHaste);
    lua_register(L, "GetVersatility", [](lua_State* L2) -> int {
        // Versatility doesn't exist in WotLK (3.3.5), added in Warlords of Draenor
        lua_pushnumber(L2, 0);
        return 1;
    });

    // ── Final batch: remaining missing functions from error log ──
    lua_register(L, "FauxScrollFrame_GetOffset", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "FauxScrollFrame_OnVerticalScroll", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "FauxScrollFrame_Update", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetChatWindowInfo", [](lua_State* L2) -> int {
        lua_pushstring(L2, "General"); lua_pushnumber(L2, 14); lua_pushnumber(L2, 1);
        lua_pushnumber(L2, 1); lua_pushnumber(L2, 0); lua_pushboolean(L2, 1);
        lua_pushboolean(L2, 0); lua_pushboolean(L2, 1);
        return 8;
    });
    lua_register(L, "SetChatWindowName", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetCVarMin", [](lua_State* L2) -> int { lua_pushstring(L2, "0"); return 1; });
    lua_register(L, "GetCVarMax", [](lua_State* L2) -> int { lua_pushstring(L2, "1"); return 1; });
    lua_register(L, "GetNetStats", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0); lua_pushnumber(L2, 0); lua_pushnumber(L2, 50); lua_pushnumber(L2, 0);
        return 4;
    });
    lua_register(L, "GetSendMailPrice", [](lua_State* L2) -> int { lua_pushnumber(L2, 30); return 1; });
    lua_register(L, "GetVoiceSessionInfo", [](lua_State* L2) -> int {
        lua_pushstring(L2, ""); lua_pushboolean(L2, 0);
        return 2;
    });
    lua_register(L, "IsVoiceChatEnabled", [](lua_State* L2) -> int { lua_pushboolean(L2, 0); return 1; });
    lua_register(L, "PanelTemplates_SetTab", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "RequestBattlefieldPositions", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetNumBattlefieldScores", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "RequestBattlefieldScoreData", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "UnitCastingInfo", [](lua_State* L2) -> int {
        // UnitCastingInfo(unit) → spell, rank, displayName, icon, startTime, endTime, isTradeSkill, castID, notInterruptible
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity && Entity->IsUnit()) {
            FWowUnitEntity* Unit = static_cast<FWowUnitEntity*>(Entity);
            // TODO: Check if unit is casting from spell state
            // For now, return nil (not casting)
        }
        return 0;
    });
    lua_register(L, "MoneyFrame_SetType", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "IsEquippedAction", [](lua_State* L2) -> int {
        // IsEquippedAction(slot) → boolean (true if action corresponds to an equipped item)
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 1)) - 1; // Convert to 0-based
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144) {
            const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
            if (Slot < ActionButtons.Num()) {
                uint32 Data = ActionButtons[Slot];
                uint8 Type = (Data >> 24) & 0xFF;
                uint32 ItemId = Data & 0x00FFFFFF;
                // TODO: Check if this item ID corresponds to an equipped item
                lua_pushboolean(L2, false);
            } else {
                lua_pushboolean(L2, false);
            }
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "ScrollFrame_OnScrollRangeChanged", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "IsConsumableAction", [](lua_State* L2) -> int {
        // IsConsumableAction(slot) → boolean (true if action is a consumable)
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 1)) - 1; // Convert to 0-based
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144) {
            const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
            if (Slot < ActionButtons.Num()) {
                uint32 Data = ActionButtons[Slot];
                uint8 Type = (Data >> 24) & 0xFF;
                // Type 1 = Item, would need to check item data for consumable flag
                lua_pushboolean(L2, Type == 1);
            } else {
                lua_pushboolean(L2, false);
            }
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsStackableAction", [](lua_State* L2) -> int {
        // IsStackableAction(slot) → boolean (true if action is stackable)
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 1)) - 1; // Convert to 0-based
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144) {
            const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
            if (Slot < ActionButtons.Num()) {
                uint32 Data = ActionButtons[Slot];
                uint8 Type = (Data >> 24) & 0xFF;
                // Items can be stackable, would need to check item data
                lua_pushboolean(L2, Type == 1);
            } else {
                lua_pushboolean(L2, false);
            }
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsItemAction", [](lua_State* L2) -> int {
        // IsItemAction(slot) → boolean (true if action is an item)
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 1)) - 1; // Convert to 0-based
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144) {
            const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
            if (Slot < ActionButtons.Num()) {
                uint32 Data = ActionButtons[Slot];
                uint8 Type = (Data >> 24) & 0xFF;
                lua_pushboolean(L2, Type == 1); // Type 1 = Item
            } else {
                lua_pushboolean(L2, false);
            }
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "GetActionBarToggles", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 1); lua_pushnumber(L2, 1); lua_pushnumber(L2, 1); lua_pushnumber(L2, 1);
        return 4;
    });

    lua_register(L, "MoneyFrame_Update", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "IsAutoRepeatAction", [](lua_State* L2) -> int {
        // IsAutoRepeatAction(slot) → boolean (true if action is auto-attack)
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 1)) - 1; // Convert to 0-based
        // Slot 1 (0-based slot 0) is usually auto-attack
        lua_pushboolean(L2, Slot == 0);
        return 1;
    });
    lua_register(L, "IsAutoRepeatSpell", [](lua_State* L2) -> int {
        // IsAutoRepeatSpell(slot) → boolean (true if spell is auto-repeat like Shoot)
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 1)) - 1; // Convert to 0-based
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144) {
            const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
            if (Slot < ActionButtons.Num()) {
                uint32 Data = ActionButtons[Slot];
                uint8 Type = (Data >> 24) & 0xFF;
                uint32 SpellId = Data & 0x00FFFFFF;
                // TODO: Check if spell is auto-repeat (like hunter Auto Shot)
                lua_pushboolean(L2, false);
            } else {
                lua_pushboolean(L2, false);
            }
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "OffhandHasWeapon", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->EntityManager)
        {
            FWowPlayerEntity* Player = Ctx->EntityManager->GetLocalPlayer();
            if (Player)
            {
                // Check offhand equipment slot (slot 16 in WoW)
                uint64 OffhandGuid = Player->GetEquipmentItemGuid(PlayerField::EQUIPMENT_SLOT_OFFHAND);
                if (OffhandGuid != 0)
                {
                    // Find the item entity and check if it's a weapon
                    FWowEntity* ItemEntity = Ctx->EntityManager->Find(OffhandGuid);
                    if (ItemEntity && ItemEntity->IsItem())
                    {
                        // Would need item template to determine weapon type
                        // For now, assume any offhand item could be a weapon
                        lua_pushboolean(L2, 1);
                        return 1;
                    }
                }
            }
        }
        lua_pushboolean(L2, 0);
        return 1;
    });
    lua_register(L, "GetActionText", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 1)) - 1; // Convert to 0-based

        if (Ctx && Ctx->ConnectionManager && Slot >= 0 && Slot < 144)
        {
            const TArray<uint32>& ActionButtons = Ctx->ConnectionManager->PacketHandler.ActionButtons;
            if (Slot < ActionButtons.Num() && ActionButtons[Slot] != 0)
            {
                uint32 ActionData = ActionButtons[Slot];
                uint8 ActionType = (ActionData >> 24) & 0xFF;
                uint32 ActionId = ActionData & 0x00FFFFFF;

                if (ActionType == 0) // Spell action
                {
                    // For spells, return empty string (spells don't have text)
                    lua_pushstring(L2, "");
                    return 1;
                }
                else if (ActionType == 64) // Macro action
                {
                    // For macros, we would need to look up macro name
                    // For now return empty as we don't have macro data structure
                    lua_pushstring(L2, "");
                    return 1;
                }
            }
        }
        lua_pushstring(L2, "");
        return 1;
    });
    lua_register(L, "GetNumVoiceSessionMembersBySessionID", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });

    // Priority 4 - Additional missing API functions that cause Lua errors
    lua_register(L, "GetSpellLink", [](lua_State* L2) -> int {
        int32 SpellId = static_cast<int32>(luaL_checknumber(L2, 1));
        
        // Look up spell name from SpellDbc
        const FSpellDbcEntry* SpellEntry = FDbcStore::Get().Spells().GetById(SpellId);
        if (SpellEntry)
        {
            // Construct WoW spell link: |cff71d5ff|Hspell:ID|h[SpellName]|h|r
            FString SpellLink = FString::Printf(TEXT("|cff71d5ff|Hspell:%d|h[%s]|h|r"), 
                SpellId, *SpellEntry->SpellName);
            lua_pushstring(L2, TCHAR_TO_UTF8(*SpellLink));
            return 1;
        }
        lua_pushnil(L2);
        return 1;
    });
    lua_register(L, "GetTradeSkillLine", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetAuctionItemInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "CanInspect", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "InspectUnit", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "CheckInteractDistance", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "GetGuildBankMoney", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "IsSpellKnown", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        int32 SpellId = static_cast<int32>(luaL_checknumber(L2, 1));

        if (Ctx && Ctx->ConnectionManager && SpellId > 0)
        {
            const TSet<uint32>& KnownSpells = Ctx->ConnectionManager->PacketHandler.KnownSpells;
            bool bKnown = KnownSpells.Contains(static_cast<uint32>(SpellId));
            lua_pushboolean(L2, bKnown ? 1 : 0);
            return 1;
        }

        lua_pushboolean(L2, 0);
        return 1;
    });
    lua_register(L, "GetSpellCharges", [](lua_State* L2) -> int {
        // Spell charges system doesn't exist in WotLK (3.3.5)
        lua_pushnil(L2);
        return 1;
    });
    lua_register(L, "GetItemCooldown", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0); lua_pushnumber(L2, 0); lua_pushnumber(L2, 0);
        return 3;
    });
    lua_register(L, "GetWeaponEnchantInfo", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false); lua_pushnil(L2); lua_pushnil(L2);
        lua_pushboolean(L2, false); lua_pushnil(L2); lua_pushnil(L2);
        return 6;
    });
    lua_register(L, "GetInventoryItemCount", [](lua_State* L2) -> int {
        FWowEntity* Entity = ResolveUnit(L2, 1);
        int32 SlotIndex = static_cast<int32>(luaL_optinteger(L2, 2, 0));

        if (!Entity || !Entity->IsPlayer())
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        FWowPlayerEntity* Player = static_cast<FWowPlayerEntity*>(Entity);

        // Get equipment item GUID for this slot (equipment slots 0-18)
        if (SlotIndex >= 0 && SlotIndex < 19)
        {
            uint64 ItemGuid = Player->GetEquipmentItemGuid(SlotIndex);
            if (ItemGuid != 0)
            {
                lua_pushnumber(L2, 1); // Equipped items count as 1
                return 1;
            }
        }

        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetInventoryItemQuality", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        FWowEntity* Entity = ResolveUnit(L2, 1);
        int32 SlotIndex = static_cast<int32>(luaL_optinteger(L2, 2, 0));

        if (!Ctx || !Ctx->EntityManager || !Entity || !Entity->IsPlayer())
        {
            lua_pushnil(L2);
            return 1;
        }

        FWowPlayerEntity* Player = static_cast<FWowPlayerEntity*>(Entity);

        // Get equipment item GUID for this slot
        if (SlotIndex >= 0 && SlotIndex < 19)
        {
            uint64 ItemGuid = Player->GetEquipmentItemGuid(SlotIndex);
            if (ItemGuid != 0)
            {
                // Try to find the item entity to get its quality
                FWowEntity* ItemEntity = Ctx->EntityManager->Find(ItemGuid);
                if (ItemEntity && ItemEntity->IsItem())
                {
                    // Quality is typically stored in item fields or we default to common
                    lua_pushnumber(L2, 1); // 1 = common (white)
                    return 1;
                }
                else
                {
                    // No item entity found but slot has item, default quality
                    lua_pushnumber(L2, 1); // 1 = common (white)
                    return 1;
                }
            }
        }

        lua_pushnil(L2);
        return 1;
    });
    lua_register(L, "IsEquippedItemType", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "GetShapeshiftFormCooldown", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0); lua_pushnumber(L2, 0); lua_pushnumber(L2, 1);
        return 3;
    });
    lua_register(L, "UnitThreatSituation", [](lua_State* L2) -> int {
        // UnitThreatSituation(unit, otherUnit) → threatStatus (0=none, 1=low, 2=medium, 3=high)
        // For now, return 0 (no threat)
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetThreatStatusColor", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0); lua_pushnumber(L2, 1); lua_pushnumber(L2, 0);
        return 3;
    });
    lua_register(L, "UnitDetailedThreatSituation", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false); lua_pushnumber(L2, 0); lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0); lua_pushnumber(L2, 0);
        return 5;
    });
    lua_register(L, "GetPetActionInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetPetActionsUsable", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "UnitInRange", [](lua_State* L2) -> int {
        // UnitInRange(unit) → boolean (true if unit is in range)
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Entity) {
            // TODO: Calculate actual distance based on spell ranges, interaction distance, etc.
            // For now, return true (assume in range)
            lua_pushboolean(L2, true);
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "SpellIsTargeting", [](lua_State* L2) -> int {
        // SpellIsTargeting() → boolean (true if in targeting mode)
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "SpellStopCasting", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager) {
            // TODO: Send CMSG_CANCEL_CAST when opcode is available
            UE_LOG(LogWowLuaStub, Log, TEXT("SpellStopCasting: canceling current cast"));
        }
        return 0;
    });
    lua_register(L, "SpellStopTargeting", [](lua_State* L2) -> int {
        // Cancel targeting mode - no-op for now but accept silently
        UE_LOG(LogWowLuaStub, Verbose, TEXT("SpellStopTargeting: stopping targeting mode"));
        return 0;
    });
    lua_register(L, "GetTotemInfo", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false); lua_pushstring(L2, ""); lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0); lua_pushnil(L2);
        return 5;
    });
    lua_register(L, "GetRuneCooldown", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0); lua_pushnumber(L2, 0); lua_pushboolean(L2, true);
        return 3;
    });
    lua_register(L, "GetRuneType", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "GetMinimapZoneText", [](lua_State* L2) -> int { lua_pushstring(L2, "Unknown"); return 1; });
    lua_register(L, "SetMapToCurrentZone", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetPlayerMapPosition2", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0); lua_pushnumber(L2, 0);
        return 2;
    });
    // UI panel toggle functions — toggle FrameXML frames by name
    auto ToggleFrameByName = [](lua_State* L2, const char* FrameName) {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (!C || !C->FrameManager) return;
        int64 H = C->FrameManager->FindFrame(FString(FrameName));
        if (H >= 0)
        {
            bool bVis = C->FrameManager->IsFrameVisible(H);
            C->FrameManager->SetFrameVisible(H, !bVis);
            UE_LOG(LogWowLuaStub, Log, TEXT("Toggle %s: %s"), UTF8_TO_TCHAR(FrameName), bVis ? TEXT("Hide") : TEXT("Show"));
        }
    };
    lua_register(L, "ToggleCharacter", [](lua_State* L2) -> int {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (C && C->FrameManager) {
            int64 H = C->FrameManager->FindFrame(TEXT("CharacterFrame"));
            if (H >= 0) { bool v = C->FrameManager->IsFrameVisible(H); C->FrameManager->SetFrameVisible(H, !v); }
        }
        return 0;
    });
    lua_register(L, "ToggleSpellBook", L_ToggleSpellBook);
    lua_register(L, "ToggleTalentFrame", [](lua_State* L2) -> int {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (C && C->FrameManager) {
            int64 H = C->FrameManager->FindFrame(TEXT("PlayerTalentFrame"));
            if (H >= 0) { bool v = C->FrameManager->IsFrameVisible(H); C->FrameManager->SetFrameVisible(H, !v); }
        }
        return 0;
    });
    lua_register(L, "ToggleFriendsFrame", [](lua_State* L2) -> int {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (C && C->FrameManager) {
            int64 H = C->FrameManager->FindFrame(TEXT("FriendsFrame"));
            if (H >= 0) { bool v = C->FrameManager->IsFrameVisible(H); C->FrameManager->SetFrameVisible(H, !v); }
        }
        return 0;
    });
    lua_register(L, "ToggleQuestLog", [](lua_State* L2) -> int {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (C && C->FrameManager) {
            int64 H = C->FrameManager->FindFrame(TEXT("QuestLogFrame"));
            if (H >= 0) { bool v = C->FrameManager->IsFrameVisible(H); C->FrameManager->SetFrameVisible(H, !v); }
        }
        return 0;
    });
    lua_register(L, "ToggleWorldMap", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "OpenWorldMap", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "CloseWorldMap", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetCoinText", [](lua_State* L2) -> int { lua_pushstring(L2, "0g"); return 1; });
    // ShowUIPanel(frame) / HideUIPanel(frame) — THE core panel management functions
    // Every WoW UI panel (spellbook, character, quest log etc.) uses these
    lua_register(L, "ShowUIPanel", [](lua_State* L2) -> int {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (!C || !C->FrameManager || !lua_istable(L2, 1)) return 0;
        lua_getfield(L2, 1, "__handle");
        if (lua_isnumber(L2, -1)) {
            int64 H = static_cast<int64>(lua_tointeger(L2, -1));
            C->FrameManager->SetFrameVisible(H, true);
        }
        lua_pop(L2, 1);
        // Also call frame:Show() via Lua so scripts fire
        lua_getfield(L2, 1, "Show");
        if (lua_isfunction(L2, -1)) {
            lua_pushvalue(L2, 1);
            lua_pcall(L2, 1, 0, 0);
        } else {
            lua_pop(L2, 1);
        }
        return 0;
    });
    lua_register(L, "HideUIPanel", [](lua_State* L2) -> int {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (!C || !C->FrameManager || !lua_istable(L2, 1)) return 0;
        lua_getfield(L2, 1, "__handle");
        if (lua_isnumber(L2, -1)) {
            int64 H = static_cast<int64>(lua_tointeger(L2, -1));
            C->FrameManager->SetFrameVisible(H, false);
        }
        lua_pop(L2, 1);
        // Also call frame:Hide() via Lua so scripts fire
        lua_getfield(L2, 1, "Hide");
        if (lua_isfunction(L2, -1)) {
            lua_pushvalue(L2, 1);
            lua_pcall(L2, 1, 0, 0);
        } else {
            lua_pop(L2, 1);
        }
        return 0;
    });
    lua_register(L, "GetUIPanel", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "CloseMenus", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "AcceptGroup", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "DeclineGroup", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "UnitIsUnit", [](lua_State* L2) -> int {
        const char* u1 = luaL_optstring(L2, 1, "");
        const char* u2 = luaL_optstring(L2, 2, "");
        lua_pushboolean(L2, strcmp(u1, u2) == 0);
        return 1;
    });
    lua_register(L, "UnitIsGroupLeader", [](lua_State* L2) -> int {
        // UnitIsGroupLeader(unit) → boolean (true if unit is group leader)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Ctx && Ctx->ConnectionManager && Entity) {
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            lua_pushboolean(L2, Entity->Guid == GroupInfo.LeaderGuid);
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "UnitIsGroupAssistant", [](lua_State* L2) -> int {
        // UnitIsGroupAssistant(unit) → boolean (true if unit is group assistant)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        FWowEntity* Entity = ResolveUnit(L2, 1);
        if (Ctx && Ctx->ConnectionManager && Entity) {
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            // TODO: Check if unit is an assistant (when that data is available)
            lua_pushboolean(L2, false);
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "UnitReaction", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->EntityManager)
        {
            lua_pushnumber(L2, 4); // neutral
            return 1;
        }

        FWowEntity* Unit1 = ResolveUnit(L2, 1);
        FWowEntity* Unit2 = ResolveUnit(L2, 2);

        if (!Unit1 || !Unit2)
        {
            lua_pushnumber(L2, 4); // neutral
            return 1;
        }

        lua_pushnumber(L2, GetUnitReactionRank(Ctx, Unit1, Unit2));
        return 1;
    });

    // Party/raid leadership
    lua_register(L, "IsPartyLeader", [](lua_State* L2) -> int {
        // IsPartyLeader() → boolean (true if player is party leader)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Ctx->EntityManager) {
            uint64 PlayerGuid = Ctx->EntityManager->LocalPlayerGuid;
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            lua_pushboolean(L2, GroupInfo.IsPartyGroup() && GroupInfo.LocalPlayerIsLeader(PlayerGuid));
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsRaidLeader", [](lua_State* L2) -> int {
        // IsRaidLeader() → boolean (true if player is raid leader)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Ctx->EntityManager) {
            uint64 PlayerGuid = Ctx->EntityManager->LocalPlayerGuid;
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            lua_pushboolean(L2, GroupInfo.IsRaidGroup() && GroupInfo.LocalPlayerIsLeader(PlayerGuid));
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "IsRaidOfficer", [](lua_State* L2) -> int {
        // IsRaidOfficer() → boolean (true if player is raid officer)
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Ctx->EntityManager) {
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            lua_pushboolean(L2, GroupInfo.IsRaidGroup() && GroupInfo.LocalPlayerIsAssistant());
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "PromoteToLeader", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "DemoteAssistant", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "UninviteUnit", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "LeaveParty", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ConvertToRaid", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ConvertToParty", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SetLootMethod", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetLootMethod", [](lua_State* L2) -> int { lua_pushstring(L2, "freeforall"); lua_pushnil(L2); lua_pushnil(L2); return 3; });
    lua_register(L, "GetLootThreshold", [](lua_State* L2) -> int { lua_pushnumber(L2, 2); return 1; });
    lua_register(L, "SetLootThreshold", [](lua_State* L2) -> int { return 0; });

    // Missing global functions from remaining errors
    lua_register(L, "GetGlobalString", [](lua_State* L2) -> int {
        const char* key = luaL_optstring(L2, 1, "");
        lua_getglobal(L2, key);
        if (lua_isnil(L2, -1)) { lua_pop(L2, 1); lua_pushstring(L2, key); }
        return 1;
    });
    lua_register(L, "IsReferAFriendLinked", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "GetOptOutOfLoot", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "HasLFGRestrictions", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "CanChangePlayerDifficulty", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "GetInstanceInfo", [](lua_State* L2) -> int {
        lua_pushnil(L2);             // name
        lua_pushstring(L2, "none");  // instanceType
        lua_pushnumber(L2, 1);       // difficulty
        lua_pushstring(L2, "");      // difficultyName
        lua_pushnumber(L2, 0);       // maxPlayers
        lua_pushnumber(L2, 0);       // playerDifficulty
        lua_pushboolean(L2, false);  // isDynamicInstance
        return 7;
    });
    lua_register(L, "GetSummonFriendCooldown", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 2;
    });
    lua_register(L, "CanSummonFriend", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "CanGrantLevel", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "SummonFriend", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GrantLevel", [](lua_State* L2) -> int { return 0; });

    // Missing functions from error logs
    lua_register(L, "UnitInBattleground", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "Sound_ChatSystem_GetNumInputDrivers", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "Sound_ChatSystem_GetInputDriverNameByIndex", [](lua_State* L2) -> int { lua_pushstring(L2, "Default"); return 1; });
    lua_register(L, "GetRefreshRates", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 60);
        return 1;
    });
    lua_register(L, "GetMultisampleFormats", [](lua_State* L2) -> int {
        // Blizzard expects colorBits, depthBits, sampleCount triples.
        lua_pushnumber(L2, 24);
        lua_pushnumber(L2, 24);
        lua_pushnumber(L2, 1);
        return 3;
    });
    lua_register(L, "GetLFGProposal", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetVoiceCurrentSessionID", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetVoiceSessionMemberInfoBySessionID", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "VoiceChat_Toggle", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "IsVoiceChatAllowedByServer", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });

    // LFG/LFR functions
    lua_register(L, "GetLFGMode", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetLFGRoles", [](lua_State* L2) -> int { lua_pushboolean(L2, false); lua_pushboolean(L2, false); lua_pushboolean(L2, false); return 3; });
    lua_register(L, "GetLFGQueueStats", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "GetBattlefieldStatus", [](lua_State* L2) -> int { lua_pushstring(L2, "none"); return 1; });

    // Tutorial
    lua_register(L, "GetTutorialsEnabled", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "SetTutorialsEnabled", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ClearTutorials", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "TriggerTutorial", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "IsTutorialFlagged", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "FlagTutorial", [](lua_State* L2) -> int { return 0; });

    // Missing video/display
    lua_register(L, "GetMaxMultisampleFormat", [](lua_State* L2) -> int { lua_pushnumber(L2, 1); return 1; });
    lua_register(L, "RestartGx", [](lua_State* L2) -> int { return 0; });

    // BattleNet
    lua_register(L, "BNGetNumFriendInvites", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "BNGetNumFriends", L_BNGetNumFriends);
    lua_register(L, "BNGetFriendInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "BNGetInfo", [](lua_State* L2) -> int { lua_pushnil(L2); lua_pushnil(L2); lua_pushnil(L2); lua_pushnil(L2); lua_pushnumber(L2, 0); lua_pushnil(L2); lua_pushnil(L2); lua_pushboolean(L2, false); lua_pushnil(L2); return 9; });
    lua_register(L, "BNSendWhisper", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "BNInviteFriend", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "BNSetCustomMessage", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "BNGetFriendInviteInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });

    // Calendar
    lua_register(L, "GetNumPendingCalendarInvites", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });

    // Missing functions that cause OnLoad errors
    lua_register(L, "GetModifiedClick", L_GetModifiedClick);
    lua_register(L, "IsModifiedClick", L_IsModifiedClick);
    lua_register(L, "SetModifiedClick", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetModifiedClickAction", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetLFGInfoServer", [](lua_State* L2) -> int {
        // Returns: isQueued, bgName, bgID, isRegistered, suspend, queueType
        lua_pushboolean(L2, false);
        lua_pushstring(L2, "");
        lua_pushnumber(L2, 0);
        lua_pushboolean(L2, false);
        lua_pushboolean(L2, false);
        lua_pushstring(L2, "none");
        return 6;
    });
    lua_register(L, "GetVoiceChatMode", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "BNGetNumFriendInvites", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetTrackingTexture", [](lua_State* L2) -> int { lua_pushstring(L2, "Interface\\Minimap\\Tracking\\None"); return 1; });
    lua_register(L, "GetInventorySlotInfo", L_GetInventorySlotInfo);

    // Rune frame (Death Knight)
    lua_register(L, "GetRuneCount", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });

    // Class color sort order table
    lua_register(L, "GetLFGRoleUpdate", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "GetNumClasses", [](lua_State* L2) -> int { lua_pushnumber(L2, 10); return 1; });
    lua_register(L, "GetClassInfo", L_GetClassInfo);

    // Remaining missing functions from OnLoad errors
    lua_register(L, "IsPartyLFG", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager)
        {
            lua_pushboolean(L2, Ctx->ConnectionManager->PacketHandler.GroupInfo.IsLfgGroup());
        }
        else
        {
            lua_pushboolean(L2, false);
        }
        return 1;
    });
    lua_register(L, "GetLFGRandomDungeonInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "BNGetFriendInfo", [](lua_State* L2) -> int {
        // Returns multiple values: presenceID, presenceName, battleTag, isBattleTagPresence, toonName, toonID, client, isOnline, lastOnline, isAFK, isDND, messageText, noteText, isRIDFriend, broadcastTime, canSoR
        for (int i = 0; i < 16; i++) lua_pushnil(L2);
        return 16;
    });
    lua_register(L, "BNGetNumFOF", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetNumDisplayChannels", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetChannelDisplayInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "IsInLFGDungeon", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "GetLFGDungeonInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "SetLFGDungeon", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "LFGGetDungeonInfoByID", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "SetLFGRoles", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GetPendingInviteConfirmations", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });

    // Functions that fix remaining OnLoad errors
    lua_register(L, "IsThreatWarningEnabled", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "GetThreatWarningEnabled", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });

    // UI toggle functions (called from keybinds — FrameXML should define these,
    // but we register fallback stubs in case the FrameXML versions failed to load)
    lua_register(L, "ToggleBackpack", [](lua_State* L2) -> int {
        FWowLuaContext* C = WowLuaApi::GetContext(L2);
        if (C && C->FrameManager) {
            // Try ContainerFrame1 (backpack)
            int64 H = C->FrameManager->FindFrame(TEXT("ContainerFrame1"));
            if (H >= 0) {
                bool v = C->FrameManager->IsFrameVisible(H);
                C->FrameManager->SetFrameVisible(H, !v);
                UE_LOG(LogWowLuaStub, Log, TEXT("ToggleBackpack: %s"), v ? TEXT("Hide") : TEXT("Show"));
            }
        }
        return 0;
    });
    lua_register(L, "ToggleSpellBook", L_ToggleSpellBook);
    // NOTE: ToggleCharacter is registered earlier with a full implementation that
    // toggles CharacterFrame visibility. Do NOT re-register here.

    // Chat frame utility functions (normally defined in ChatFrame.lua)
    lua_register(L, "ChatEdit_SetLastActiveWindow", [](lua_State* L2) -> int {
        // ChatEdit_SetLastActiveWindow(editBox) - store active chat edit box reference
        if (!lua_isnil(L2, 1)) {
            lua_pushvalue(L2, 1);
            lua_setglobal(L2, "_LastActiveChatEditBox");
            UE_LOG(LogWowLuaStub, Verbose, TEXT("ChatEdit_SetLastActiveWindow: stored active edit box"));
        }
        return 0;
    });
    lua_register(L, "ChatEdit_GetActiveWindow", [](lua_State* L2) -> int {
        // ChatEdit_GetActiveWindow() → editBox or nil
        lua_getglobal(L2, "_LastActiveChatEditBox");
        if (lua_isnil(L2, -1)) {
            // Return ChatFrame1EditBox as fallback
            lua_getglobal(L2, "ChatFrame1EditBox");
        }
        return 1;
    });
    lua_register(L, "ChatEdit_SetLastTellTarget", [](lua_State* L2) -> int {
        // ChatEdit_SetLastTellTarget(name) - store last whisper target
        const char* Name = luaL_checkstring(L2, 1);
        lua_pushstring(L2, Name);
        lua_setglobal(L2, "_LastTellTarget");
        UE_LOG(LogWowLuaStub, Verbose, TEXT("ChatEdit_SetLastTellTarget: %s"), UTF8_TO_TCHAR(Name));
        return 0;
    });
	    lua_register(L, "FCF_GetCurrentChatFrame", [](lua_State* L2) -> int {
	        lua_getglobal(L2, "ChatFrame1");
	        return 1;
	    });
	    lua_register(L, "SetChatWindowShown", [](lua_State* L2) -> int {
	        luaL_optinteger(L2, 1, 1);
	        lua_toboolean(L2, 2);
	        return 0;
	    });
	    lua_register(L, "FCFDock_GetChatFrames", [](lua_State* L2) -> int {
	        lua_newtable(L2);
	        return 1;
	    });
    lua_register(L, "GetVoiceChatMode", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });

    // Critical functions needed by FrameXML Lua files during load
    lua_register(L, "FillLocalizedClassList", L_FillLocalizedClassList);
    lua_register(L, "RegisterStaticConstants", [](lua_State* L2) -> int { return 0; });

    // Trade system
    lua_register(L, "GetCursorMoney", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetPlayerTradeMoney", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetTargetTradeMoney", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetTradePlayerItemLink", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetTradeTargetItemLink", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetTradePlayerItemInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetTradeTargetItemInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetNumTradeItems", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });

    // Container/item operations
    lua_register(L, "PickupContainerItem", [](lua_State* L2) -> int {
        int32 Bag = static_cast<int32>(luaL_checknumber(L2, 1));
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 2));
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager) {
            // TODO: Send CMSG_AUTOSTORE_BAG_ITEM when available
            UE_LOG(LogWowLuaStub, Log, TEXT("PickupContainerItem: bag %d slot %d"), Bag, Slot);
        }
        return 0;
    });
    lua_register(L, "UseContainerItem", [](lua_State* L2) -> int {
        int32 Bag = static_cast<int32>(luaL_checknumber(L2, 1));
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 2));
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager) {
            // TODO: Send CMSG_USE_ITEM when available
            UE_LOG(LogWowLuaStub, Log, TEXT("UseContainerItem: bag %d slot %d"), Bag, Slot);
        }
        return 0;
    });
    lua_register(L, "SplitContainerItem", [](lua_State* L2) -> int {
        int32 Bag = static_cast<int32>(luaL_checknumber(L2, 1));
        int32 Slot = static_cast<int32>(luaL_checknumber(L2, 2));
        int32 Count = static_cast<int32>(luaL_checknumber(L2, 3));
        UE_LOG(LogWowLuaStub, Log, TEXT("SplitContainerItem: bag %d slot %d count %d"), Bag, Slot, Count);
        return 0;
    });
    lua_register(L, "PickupInventoryItem", [](lua_State* L2) -> int {
        int32 InvSlot = static_cast<int32>(luaL_checknumber(L2, 1));
        UE_LOG(LogWowLuaStub, Log, TEXT("PickupInventoryItem: slot %d"), InvSlot);
        return 0;
    });

    // Shaman multi-cast bar
    lua_register(L, "GetMultiCastBarOffset", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });

    // Chat window saved state (persistence)
    lua_register(L, "GetChatWindowSavedDimensions", [](lua_State* L2) -> int {
        // Returns: width, height (WoW units)
        lua_pushnumber(L2, 430); // default chat width
        lua_pushnumber(L2, 120); // default chat height
        return 2;
    });
    lua_register(L, "GetChatWindowSavedPosition", [](lua_State* L2) -> int {
        // WoW 3.3.5: GetChatWindowSavedPosition(index) -> point, xOffset, yOffset, shown
        int32 Index = static_cast<int32>(luaL_optnumber(L2, 1, 1));
        lua_pushstring(L2, "BOTTOMLEFT"); // point
        lua_pushnumber(L2, 32);           // xOffset
        lua_pushnumber(L2, 215);          // yOffset
        lua_pushboolean(L2, Index == 1 ? 1 : 0); // shown (only ChatFrame1 visible by default)
        return 4;
    });
    lua_register(L, "SetChatWindowSavedDimensions", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SetChatWindowSavedPosition", [](lua_State* L2) -> int { return 0; });

    // Party member lookup
    lua_register(L, "GetPartyMember", [](lua_State* L2) -> int {
        int32 Index = static_cast<int32>(luaL_optnumber(L2, 1, 0));
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager && Index >= 1 && Index <= 4)
        {
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            int32 PartyIdx = Index - 1;
            if (PartyIdx < GroupInfo.Members.Num())
            {
                lua_pushnumber(L2, PartyIdx + 1);
                return 1;
            }
        }
        lua_pushnil(L2);
        return 1;
    });
    lua_register(L, "GetChatTypeIndex", [](lua_State* L2) -> int {
        // GetChatTypeIndex(chatType) -> index
        const char* chatType = luaL_optstring(L2, 1, "");
        // Return a valid index for common chat types
        lua_pushnumber(L2, 1);
        return 1;
    });
    lua_register(L, "GetNumEquipmentSets", [](lua_State* L2) -> int { lua_pushnumber(L2, 0); return 1; });
    lua_register(L, "GetEquipmentSetInfo", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });

    // Ready check
    lua_register(L, "GetReadyCheckStatus", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->ConnectionManager || !Ctx->EntityManager)
        {
            lua_pushnil(L2);
            return 1;
        }

        const uint64 UnitGuid = ResolveUnitGuid(L2, 1);
        if (UnitGuid == 0)
        {
            lua_pushnil(L2);
            return 1;
        }

        const FWowPacketHandler& PacketHandler = Ctx->ConnectionManager->PacketHandler;
        if (!PacketHandler.ReadyCheck.bActive)
        {
            lua_pushnil(L2);
            return 1;
        }

        const bool bIsLocalPlayer = UnitGuid == Ctx->EntityManager->LocalPlayerGuid;
        const FWowGroupMember* Member = bIsLocalPlayer ? nullptr : PacketHandler.GroupInfo.FindMember(UnitGuid);
        const bool bOnline = bIsLocalPlayer
            || (Member && (Member->Status & WowGroupMemberStatus::ONLINE) != 0);

        const uint8* ResponseState = PacketHandler.ReadyCheck.FindResponse(UnitGuid);
        if (!bOnline && ResponseState == nullptr)
        {
            lua_pushstring(L2, "notready");
            return 1;
        }

        if (ResponseState == nullptr)
        {
            lua_pushstring(L2, "waiting");
            return 1;
        }

        lua_pushstring(L2, (*ResponseState == WowReadyCheckResponse::READY) ? "ready" : "notready");
        return 1;
    });
    lua_register(L, "GetReadyCheckTimeLeft", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->ConnectionManager)
        {
            lua_pushnumber(L2, 0);
            return 1;
        }

        lua_pushnumber(L2, Ctx->ConnectionManager->PacketHandler.ReadyCheck.GetTimeLeftSeconds());
        return 1;
    });
    lua_register(L, "GetRaidTargetIndex", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (!Ctx || !Ctx->ConnectionManager)
        {
            lua_pushnil(L2);
            return 1;
        }

        const uint64 UnitGuid = ResolveUnitGuid(L2, 1);
        if (UnitGuid == 0)
        {
            lua_pushnil(L2);
            return 1;
        }

        const int32 IconIndex = Ctx->ConnectionManager->PacketHandler.RaidTargets.GetIconIndex(UnitGuid);
        if (IconIndex == INDEX_NONE)
        {
            lua_pushnil(L2);
            return 1;
        }

        lua_pushnumber(L2, IconIndex + 1);
        return 1;
    });
    lua_register(L, "DoReadyCheck", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager)
        {
            Ctx->ConnectionManager->SendStartReadyCheck();
        }
        return 0;
    });
    lua_register(L, "ConfirmReadyCheck", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager)
        {
            Ctx->ConnectionManager->SendReadyCheckConfirm(lua_toboolean(L2, 1) != 0);
        }
        return 0;
    });

    // Vehicle UI
    lua_register(L, "UnitHasVehicleUI", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "UnitControllingVehicle", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "UnitUsingVehicle", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "CanExitVehicle", [](lua_State* L2) -> int { lua_pushboolean(L2, false); return 1; });
    lua_register(L, "GetVehicleUIIndicator", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });
    lua_register(L, "GetVehicleUIIndicatorSeat", [](lua_State* L2) -> int { lua_pushnil(L2); return 1; });

    // Party leader functions
    lua_register(L, "GetPartyLeaderIndex", [](lua_State* L2) -> int {
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager)
        {
            const FWowGroupInfo& GroupInfo = Ctx->ConnectionManager->PacketHandler.GroupInfo;
            if (!GroupInfo.IsEmpty() && GroupInfo.LeaderGuid != 0)
            {
                // Find the leader in the members array (1-based indexing for Lua)
                for (int32 i = 0; i < GroupInfo.Members.Num(); ++i)
                {
                    if (GroupInfo.Members[i].Guid == GroupInfo.LeaderGuid)
                    {
                        lua_pushnumber(L2, i + 1); // Convert to 1-based
                        return 1;
                    }
                }
            }
        }
        lua_pushnumber(L2, 0); // Not in party or no leader
        return 1;
    });

    // Bank slot functions
    lua_register(L, "GetNumBankSlots", [](lua_State* L2) -> int {
        int32 PurchasedSlots = 0;
        FWowLuaContext* Ctx = WowLuaApi::GetContext(L2);
        if (Ctx && Ctx->ConnectionManager)
        {
            if (const FWowPlayerEntity* Player = Ctx->ConnectionManager->PacketHandler.EntityManager.GetLocalPlayer())
            {
                PurchasedSlots = Player->GetBankBagSlotCount();
            }
        }

        lua_pushnumber(L2, PurchasedSlots);
        lua_pushboolean(L2, PurchasedSlots >= 7);
        return 2;
    });

    // CreateFont function - creates a font object with frame metatable
    lua_register(L, "CreateFont", [](lua_State* L2) -> int {
        const char* fontName = luaL_optstring(L2, 1, "DefaultFont");

        // Create a font table with basic properties
        lua_newtable(L2);

        // Set font name
        lua_pushstring(L2, fontName);
        lua_setfield(L2, -2, "fontName");

        // Add basic font methods (stubs for now)
        lua_pushcfunction(L2, [](lua_State* L3) -> int {
            // SetFont(fontPath, size, flags)
            return 0;
        });
        lua_setfield(L2, -2, "SetFont");

        lua_pushcfunction(L2, [](lua_State* L3) -> int {
            // GetFont() -> fontPath, size, flags
            lua_pushstring(L3, "Fonts\\FRIZQT__.TTF");
            lua_pushnumber(L3, 12);
            lua_pushstring(L3, "");
            return 3;
        });
        lua_setfield(L2, -2, "GetFont");

        return 1;
    });

    // ── Functions needed by OnUpdate/OnEvent handlers (function="..." attribute) ──

    // Achievement system
    lua_register(L, "HasCompletedAnyAchievement", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "GetAchievementInfo", [](lua_State* L2) -> int {
        // Returns: id, name, points, completed, month, day, year, description, flags, icon, rewardText, isGuild, wasEarnedByMe, earnedBy
        lua_pushnumber(L2, luaL_optnumber(L2, 1, 0));
        lua_pushstring(L2, "");
        lua_pushnumber(L2, 0);
        lua_pushboolean(L2, false);
        lua_pushnil(L2); lua_pushnil(L2); lua_pushnil(L2);
        lua_pushstring(L2, "");
        lua_pushnumber(L2, 0);
        lua_pushstring(L2, "Interface\\Icons\\INV_Misc_QuestionMark");
        lua_pushstring(L2, "");
        lua_pushboolean(L2, false);
        lua_pushboolean(L2, false);
        lua_pushnil(L2);
        return 14;
    });

    // Voice chat
    lua_register(L, "VoiceChat_IsRecordingLoopbackSound", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "VoiceChat_IsPlayingLoopbackSound", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "VoiceChat_GetCurrentMicrophoneSignalLevel", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });

    // Calendar
    lua_register(L, "CalendarGetDate", [](lua_State* L2) -> int {
        // Returns: weekday, month, day, year
        FDateTime Now = FDateTime::Now();
        lua_pushnumber(L2, static_cast<int>(Now.GetDayOfWeek()) + 1);
        lua_pushnumber(L2, Now.GetMonth());
        lua_pushnumber(L2, Now.GetDay());
        lua_pushnumber(L2, Now.GetYear());
        return 4;
    });
    lua_register(L, "CalendarGetNumPendingInvites", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });

    // Guild
    lua_register(L, "GuildControlGetNumRanks", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GuildControlGetRankName", [](lua_State* L2) -> int {
        lua_pushstring(L2, "");
        return 1;
    });

    // World map
    lua_register(L, "UpdateWorldMapArrowFrames", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "PositionWorldMapArrowFrame", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "ShowWorldMapArrowFrame", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "HideWorldMapArrowFrame", [](lua_State* L2) -> int { return 0; });

    // PVP / Wintergrasp
    lua_register(L, "GetWintergraspWaitTime", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "CanQueueForWintergrasp", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });

    // PVP battlefield
    lua_register(L, "PlayerIsPVPInactive", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "GetNumBattlefieldPositions", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetBattlefieldPosition", [](lua_State* L2) -> int {
        // Returns: x, y
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 2;
    });
    lua_register(L, "GetNumBattlefieldFlagPositions", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetBattlefieldFlagPosition", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushstring(L2, "");
        return 3;
    });
    lua_register(L, "GetCorpseMapPosition", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 2;
    });
    lua_register(L, "GetDeathReleasePosition", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 2;
    });
    lua_register(L, "GetCurrentMapContinent", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });
    lua_register(L, "GetCurrentMapZone", [](lua_State* L2) -> int {
        lua_pushnumber(L2, 0);
        return 1;
    });

    // Container/bag
    lua_register(L, "GetContainerNumFreeSlots", [](lua_State* L2) -> int {
        // Returns: numFreeSlots, bagType
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 2;
    });

    // GM ticket system
    lua_register(L, "GetGMTicket", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "GMEuropaBugsEnabled", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });

    // Voice system
    lua_register(L, "GetVoiceStatus", [](lua_State* L2) -> int {
        // Returns: isEnabled, isTalking
        lua_pushboolean(L2, false);
        lua_pushboolean(L2, false);
        return 2;
    });
    lua_register(L, "GetMuteStatus", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "UnitIsTalking", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "UnitIsMuted", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });

    // Spell targeting
    lua_register(L, "SpellCanTargetItem", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "SpellCanTargetUnit", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "SpellIsTargeting", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "SpellTargetUnit", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SpellStopTargeting", [](lua_State* L2) -> int { return 0; });
    lua_register(L, "SpellStopCasting", [](lua_State* L2) -> int { return 0; });

    // Unit state queries
    lua_register(L, "UnitIsCorpse", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "UnitIsGhost", [](lua_State* L2) -> int {
        lua_pushboolean(L2, false);
        return 1;
    });
    lua_register(L, "UnitIsDeadOrGhost", [](lua_State* L2) -> int {
        FWowEntity* Entity = ResolveUnit(L2);
        if (Entity && Entity->IsUnit()) {
            const FWowUnitEntity* Unit = static_cast<const FWowUnitEntity*>(Entity);
            lua_pushboolean(L2, Unit->IsDead());
        } else {
            lua_pushboolean(L2, false);
        }
        return 1;
    });

    // Buff frame needs GetWeaponEnchantInfo for timeLeft
    lua_register(L, "GetWeaponEnchantInfo", [](lua_State* L2) -> int {
        // Returns: hasMainHandEnchant, mainHandExpiration, mainHandCharges, hasOffHandEnchant, offHandExpiration, offHandCharges
        lua_pushboolean(L2, false);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        lua_pushboolean(L2, false);
        lua_pushnumber(L2, 0);
        lua_pushnumber(L2, 0);
        return 6;
    });

    UE_LOG(LogWowLuaStub, Log, TEXT("Registered WoW Lua API (~400+ functions, entity-backed unit API, FrameXML-ready)"));
}

#else
void WowLuaApi::RegisterStubs(lua_State*) {}
#endif
