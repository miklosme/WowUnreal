#include "Formats/Dbc/SpellVisualKitDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpellVisualKitDbc, Log, All);

bool FSpellVisualKitDbc::Load(const FDbcParser& Parser)
{
	if (!Parser.IsValid())
	{
		return false;
	}

	Entries.Reset();
	IdIndex.Reset();
	Entries.Reserve(Parser.GetRecordCount());

	for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
	{
		FSpellVisualKitDbcEntry E;
		E.ID = Parser.GetUInt(i, 0);
		E.StartAnimID = Parser.GetUInt(i, 1);
		E.AnimID = Parser.GetUInt(i, 2);
		E.AnimKitID = Parser.GetUInt(i, 3);
		E.HeadEffectID = Parser.GetUInt(i, 4);
		E.ChestEffectID = Parser.GetUInt(i, 5);
		E.BaseEffectID = Parser.GetUInt(i, 6);
		E.LeftHandEffectID = Parser.GetUInt(i, 7);
		E.RightHandEffectID = Parser.GetUInt(i, 8);
		E.BreathEffectID = Parser.GetUInt(i, 9);
		E.LeftWeaponEffectID = Parser.GetUInt(i, 10);
		E.RightWeaponEffectID = Parser.GetUInt(i, 11);
		E.SoundID = Parser.GetUInt(i, 12);
		E.ShakeID = Parser.GetUInt(i, 13);

		IdIndex.Add(E.ID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogSpellVisualKitDbc, Log, TEXT("Loaded SpellVisualKit.dbc: %d records"), Entries.Num());
	return true;
}

const FSpellVisualKitDbcEntry* FSpellVisualKitDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}