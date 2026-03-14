#pragma once
#include "CoreMinimal.h"

// WoW 3.3.5a update field indices (build 12340)
// Reference: azerothcore-wotlk UpdateFields.h

// Object types (OBJECT_FIELD_TYPE bitmask)
namespace WowTypeMask
{
    inline constexpr uint32 OBJECT        = 0x0001;
    inline constexpr uint32 ITEM          = 0x0002;
    inline constexpr uint32 CONTAINER     = 0x0004;
    inline constexpr uint32 UNIT          = 0x0008;
    inline constexpr uint32 PLAYER        = 0x0010;
    inline constexpr uint32 GAMEOBJECT    = 0x0020;
    inline constexpr uint32 DYNAMICOBJECT = 0x0040;
    inline constexpr uint32 CORPSE        = 0x0080;
}

// GUID high-type masks for 64-bit GUIDs
namespace WowHighGuid
{
    inline constexpr uint64 PLAYER        = 0x0000000000000000ULL;
    inline constexpr uint64 ITEM          = 0x4000000000000000ULL;
    inline constexpr uint64 GAMEOBJECT    = 0xF110000000000000ULL;
    inline constexpr uint64 TRANSPORT     = 0xF120000000000000ULL;
    inline constexpr uint64 UNIT          = 0xF130000000000000ULL;
    inline constexpr uint64 PET           = 0xF140000000000000ULL;
    inline constexpr uint64 VEHICLE       = 0xF150000000000000ULL;
    inline constexpr uint64 DYNAMICOBJECT = 0xF100000000000000ULL;
    inline constexpr uint64 CORPSE        = 0xF101000000000000ULL;
}

// Object base fields (all objects have these)
namespace ObjectField
{
    inline constexpr uint16 GUID        = 0x0000; // 2 uint32s (64-bit)
    inline constexpr uint16 TYPE        = 0x0002;
    inline constexpr uint16 ENTRY       = 0x0003;
    inline constexpr uint16 SCALE_X     = 0x0004;
    inline constexpr uint16 PADDING     = 0x0005;
    inline constexpr uint16 END         = 0x0006;
}

// Item fields (offset from OBJECT_END)
namespace ItemField
{
    inline constexpr uint16 BASE = ObjectField::END;

    inline constexpr uint16 OWNER                = BASE + 0x0000; // 2
    inline constexpr uint16 CONTAINED            = BASE + 0x0002; // 2
    inline constexpr uint16 CREATOR              = BASE + 0x0004; // 2
    inline constexpr uint16 GIFT_CREATOR         = BASE + 0x0006; // 2
    inline constexpr uint16 STACK_COUNT          = BASE + 0x0008;
    inline constexpr uint16 DURATION             = BASE + 0x0009;
    inline constexpr uint16 SPELL_CHARGES        = BASE + 0x000A; // 5
    inline constexpr uint16 FLAGS                = BASE + 0x000F;
    inline constexpr uint16 ENCHANTMENT_1_1      = BASE + 0x0010;
    inline constexpr uint16 ENCHANTMENT_1_3      = BASE + 0x0012;
    inline constexpr uint16 ENCHANTMENT_2_1      = BASE + 0x0013;
    inline constexpr uint16 ENCHANTMENT_2_3      = BASE + 0x0015;
    inline constexpr uint16 ENCHANTMENT_3_1      = BASE + 0x0016;
    inline constexpr uint16 ENCHANTMENT_3_3      = BASE + 0x0018;
    inline constexpr uint16 ENCHANTMENT_4_1      = BASE + 0x0019;
    inline constexpr uint16 ENCHANTMENT_4_3      = BASE + 0x001B;
    inline constexpr uint16 ENCHANTMENT_5_1      = BASE + 0x001C;
    inline constexpr uint16 ENCHANTMENT_5_3      = BASE + 0x001E;
    inline constexpr uint16 ENCHANTMENT_6_1      = BASE + 0x001F;
    inline constexpr uint16 ENCHANTMENT_6_3      = BASE + 0x0021;
    inline constexpr uint16 ENCHANTMENT_7_1      = BASE + 0x0022;
    inline constexpr uint16 ENCHANTMENT_7_3      = BASE + 0x0024;
    inline constexpr uint16 ENCHANTMENT_8_1      = BASE + 0x0025;
    inline constexpr uint16 ENCHANTMENT_8_3      = BASE + 0x0027;
    inline constexpr uint16 ENCHANTMENT_9_1      = BASE + 0x0028;
    inline constexpr uint16 ENCHANTMENT_9_3      = BASE + 0x002A;
    inline constexpr uint16 ENCHANTMENT_10_1     = BASE + 0x002B;
    inline constexpr uint16 ENCHANTMENT_10_3     = BASE + 0x002D;
    inline constexpr uint16 ENCHANTMENT_11_1     = BASE + 0x002E;
    inline constexpr uint16 ENCHANTMENT_11_3     = BASE + 0x0030;
    inline constexpr uint16 ENCHANTMENT_12_1     = BASE + 0x0031;
    inline constexpr uint16 ENCHANTMENT_12_3     = BASE + 0x0033;
    inline constexpr uint16 PROPERTY_SEED        = BASE + 0x0034;
    inline constexpr uint16 RANDOM_PROPERTIES_ID = BASE + 0x0035;
    inline constexpr uint16 DURABILITY           = BASE + 0x0036;
    inline constexpr uint16 MAX_DURABILITY       = BASE + 0x0037;
    inline constexpr uint16 CREATE_PLAYED_TIME   = BASE + 0x0038;

    inline constexpr uint16 END = BASE + 0x003A;
}

// Container fields (offset from ITEM_END)
namespace ContainerField
{
    inline constexpr uint16 BASE = ItemField::END;

    inline constexpr uint16 NUM_SLOTS = BASE + 0x0000;
    inline constexpr uint16 ALIGN_PAD = BASE + 0x0001;
    inline constexpr uint16 SLOT_1    = BASE + 0x0002; // 72 uint32s / 36 GUIDs

    inline constexpr uint16 END = BASE + 0x004A;
}

// Unit fields (offset from OBJECT_END)
namespace UnitField
{
    inline constexpr uint16 BASE = ObjectField::END;

    inline constexpr uint16 CHARM                = BASE + 0x0000; // 2
    inline constexpr uint16 SUMMON               = BASE + 0x0002; // 2
    inline constexpr uint16 CRITTER              = BASE + 0x0004; // 2
    inline constexpr uint16 CHARMEDBY            = BASE + 0x0006; // 2
    inline constexpr uint16 SUMMONEDBY           = BASE + 0x0008; // 2
    inline constexpr uint16 CREATEDBY            = BASE + 0x000A; // 2
    inline constexpr uint16 TARGET               = BASE + 0x000C; // 2
    inline constexpr uint16 CHANNEL_OBJECT       = BASE + 0x000E; // 2
    inline constexpr uint16 CHANNEL_SPELL        = BASE + 0x0010;
    inline constexpr uint16 BYTES_0              = BASE + 0x0011;
    inline constexpr uint16 HEALTH               = BASE + 0x0012;
    inline constexpr uint16 POWER1               = BASE + 0x0013; // Mana
    inline constexpr uint16 POWER2               = BASE + 0x0014; // Rage
    inline constexpr uint16 POWER3               = BASE + 0x0015; // Focus
    inline constexpr uint16 POWER4               = BASE + 0x0016; // Energy
    inline constexpr uint16 POWER5               = BASE + 0x0017; // Happiness
    inline constexpr uint16 POWER6               = BASE + 0x0018;
    inline constexpr uint16 POWER7               = BASE + 0x0019;
    inline constexpr uint16 MAXHEALTH            = BASE + 0x001A;
    inline constexpr uint16 MAXPOWER1            = BASE + 0x001B;
    inline constexpr uint16 MAXPOWER2            = BASE + 0x001C;
    inline constexpr uint16 MAXPOWER3            = BASE + 0x001D;
    inline constexpr uint16 MAXPOWER4            = BASE + 0x001E;
    inline constexpr uint16 MAXPOWER5            = BASE + 0x001F;
    inline constexpr uint16 LEVEL                = BASE + 0x0030;
    inline constexpr uint16 FACTIONTEMPLATE      = BASE + 0x0031;
    inline constexpr uint16 FLAGS                = BASE + 0x0035;
    inline constexpr uint16 FLAGS_2              = BASE + 0x0036;
    inline constexpr uint16 DISPLAYID            = BASE + 0x003D;
    inline constexpr uint16 NATIVEDISPLAYID      = BASE + 0x003E;
    inline constexpr uint16 MOUNTDISPLAYID       = BASE + 0x003F;

    inline constexpr uint16 END = BASE + 0x008E;
}

// Player fields (offset from UNIT_END) — subset
namespace PlayerField
{
    inline constexpr uint16 BASE = UnitField::END;

    inline constexpr uint16 FLAGS         = BASE + 0x0002;
    inline constexpr uint16 BYTES         = BASE + 0x0005;
    inline constexpr uint16 XP            = BASE + 0x01E6;
    inline constexpr uint16 NEXT_LEVEL_XP = BASE + 0x01E7;
    inline constexpr uint16 COINAGE       = BASE + 0x03FE;

    inline constexpr uint16 END = BASE + 0x0474;
}

// GameObject fields (offset from OBJECT_END)
namespace GameObjectField
{
    inline constexpr uint16 BASE = ObjectField::END;

    inline constexpr uint16 CREATED_BY      = BASE + 0x0000; // 2
    inline constexpr uint16 DISPLAY_ID      = BASE + 0x0002;
    inline constexpr uint16 FLAGS           = BASE + 0x0003;
    inline constexpr uint16 PARENT_ROTATION = BASE + 0x0004; // 4 floats
    inline constexpr uint16 DYNAMIC         = BASE + 0x0008;
    inline constexpr uint16 FACTION         = BASE + 0x0009;
    inline constexpr uint16 LEVEL           = BASE + 0x000A;
    inline constexpr uint16 BYTES_1         = BASE + 0x000B;

    inline constexpr uint16 END = BASE + 0x000C;
}

// DynamicObject fields (offset from OBJECT_END)
namespace DynamicObjectField
{
    inline constexpr uint16 BASE = ObjectField::END;

    inline constexpr uint16 CASTER    = BASE + 0x0000; // 2
    inline constexpr uint16 BYTES     = BASE + 0x0002;
    inline constexpr uint16 SPELL_ID  = BASE + 0x0003;
    inline constexpr uint16 RADIUS    = BASE + 0x0004;
    inline constexpr uint16 CAST_TIME = BASE + 0x0005;

    inline constexpr uint16 END = BASE + 0x0006;
}

// Corpse fields (offset from OBJECT_END)
namespace CorpseField
{
    inline constexpr uint16 BASE = ObjectField::END;

    inline constexpr uint16 OWNER = BASE + 0x0000; // 2

    inline constexpr uint16 END = BASE + 0x0012;
}

// UpdateObject block types
namespace UpdateType
{
    inline constexpr uint8 VALUES               = 0;
    inline constexpr uint8 MOVEMENT             = 1;
    inline constexpr uint8 CREATE_OBJECT        = 2;
    inline constexpr uint8 CREATE_OBJECT2       = 3;
    inline constexpr uint8 OUT_OF_RANGE_OBJECTS  = 4;
    inline constexpr uint8 NEAR_OBJECTS          = 5;
}

// Object update flags
namespace UpdateFlag
{
    inline constexpr uint16 NONE                = 0x0000;
    inline constexpr uint16 SELF                = 0x0001;
    inline constexpr uint16 TRANSPORT           = 0x0002;
    inline constexpr uint16 HAS_TARGET          = 0x0004;
    inline constexpr uint16 LOWGUID             = 0x0010;
    inline constexpr uint16 LIVING              = 0x0020;
    inline constexpr uint16 STATIONARY_POSITION = 0x0040;
    inline constexpr uint16 VEHICLE             = 0x0080;
    inline constexpr uint16 POSITION            = 0x0100;
    inline constexpr uint16 ROTATION            = 0x0200;
}
