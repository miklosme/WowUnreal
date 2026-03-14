#include "Formats/Dbc/SpellDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpellDbc, Log, All);

bool FSpellDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid())
    {
        return false;
    }

    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FSpellDbcEntry E;

        E.ID = Parser.GetUInt(i, 0);
        E.Category = Parser.GetUInt(i, 1);
        E.Dispel = Parser.GetUInt(i, 2);
        E.Mechanic = Parser.GetUInt(i, 3);
        E.Attributes = Parser.GetUInt(i, 4);
        E.AttributesEx = Parser.GetUInt(i, 5);
        E.AttributesEx2 = Parser.GetUInt(i, 6);
        E.AttributesEx3 = Parser.GetUInt(i, 7);
        E.AttributesEx4 = Parser.GetUInt(i, 8);
        E.AttributesEx5 = Parser.GetUInt(i, 9);
        E.AttributesEx6 = Parser.GetUInt(i, 10);
        E.AttributesEx7 = Parser.GetUInt(i, 11);

        // Combine uint64 fields from two uint32 columns
        E.Stances = static_cast<uint64>(Parser.GetUInt(i, 12)) | (static_cast<uint64>(Parser.GetUInt(i, 13)) << 32);
        E.StancesNot = static_cast<uint64>(Parser.GetUInt(i, 14)) | (static_cast<uint64>(Parser.GetUInt(i, 15)) << 32);

        E.Targets = Parser.GetUInt(i, 16);
        E.TargetCreatureType = Parser.GetUInt(i, 17);
        E.RequiresSpellFocus = Parser.GetUInt(i, 18);
        E.FacingCasterFlags = Parser.GetUInt(i, 19);
        E.CasterAuraState = Parser.GetUInt(i, 20);
        E.TargetAuraState = Parser.GetUInt(i, 21);
        E.CastingTimeIndex = Parser.GetUInt(i, 28);
        E.RecoveryTime = Parser.GetUInt(i, 29);
        E.CategoryRecoveryTime = Parser.GetUInt(i, 30);
        E.MaxLevel = Parser.GetUInt(i, 37);
        E.BaseLevel = Parser.GetUInt(i, 38);
        E.SpellLevel = Parser.GetUInt(i, 39);
        E.DurationIndex = Parser.GetUInt(i, 40);
        E.PowerType = Parser.GetUInt(i, 41);
        E.ManaCost = Parser.GetUInt(i, 42);
        E.RangeIndex = Parser.GetUInt(i, 46);
        E.Speed = Parser.GetFloat(i, 47);
        E.StackAmount = Parser.GetUInt(i, 49);

        // Effect arrays
        for (int32 j = 0; j < 3; ++j)
        {
            E.Effect[j] = Parser.GetUInt(i, 71 + j);
            E.EffectBasePoints[j] = static_cast<int32>(Parser.GetUInt(i, 80 + j));
            E.EffectImplicitTargetA[j] = Parser.GetUInt(i, 86 + j);
            E.EffectImplicitTargetB[j] = Parser.GetUInt(i, 89 + j);
            E.EffectApplyAuraName[j] = Parser.GetUInt(i, 95 + j);
            E.EffectMiscValue[j] = static_cast<int32>(Parser.GetUInt(i, 110 + j));
            E.EffectTriggerSpell[j] = Parser.GetUInt(i, 116 + j);
        }

        E.SpellVisual[0] = Parser.GetUInt(i, 131);
        E.SpellVisual[1] = Parser.GetUInt(i, 132);
        E.SpellIconID = Parser.GetUInt(i, 133);
        E.ActiveIconID = Parser.GetUInt(i, 134);
        E.SpellName = Parser.GetString(i, 136);
        E.Rank = Parser.GetString(i, 153);
        E.Description = Parser.GetString(i, 170);
        E.ToolTip = Parser.GetString(i, 187);
        E.ManaCostPercentage = Parser.GetUInt(i, 204);
        E.SpellFamilyName = Parser.GetUInt(i, 208);
        E.DmgClass = Parser.GetUInt(i, 213);
        E.SchoolMask = Parser.GetUInt(i, 225);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogSpellDbc, Log, TEXT("Loaded Spell.dbc: %d records"), Entries.Num());
    return true;
}

const FSpellDbcEntry* FSpellDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}