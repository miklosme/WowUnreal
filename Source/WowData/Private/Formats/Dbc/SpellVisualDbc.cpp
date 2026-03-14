#include "Formats/Dbc/SpellVisualDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpellVisualDbc, Log, All);

bool FSpellVisualDbc::Load(const FDbcParser& Parser)
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
		FSpellVisualDbcEntry E;
		E.ID = Parser.GetUInt(i, 0);
		E.PrecastKit = Parser.GetUInt(i, 1);
		E.CastKit = Parser.GetUInt(i, 2);
		E.ImpactKit = Parser.GetUInt(i, 3);
		E.StateKit = Parser.GetUInt(i, 4);
		E.StateDoneKit = Parser.GetUInt(i, 5);
		E.ChannelKit = Parser.GetUInt(i, 6);
		E.HasMissile = Parser.GetUInt(i, 7);
		E.MissileModel = static_cast<int32>(Parser.GetUInt(i, 8));
		E.MissilePathType = Parser.GetUInt(i, 9);
		E.MissileDestinationAttachment = Parser.GetUInt(i, 10);
		E.MissileCastOffsetX = Parser.GetFloat(i, 11);
		E.MissileCastOffsetY = Parser.GetFloat(i, 12);
		E.MissileCastOffsetZ = Parser.GetFloat(i, 13);
		E.MissileImpactOffsetX = Parser.GetFloat(i, 14);
		E.MissileImpactOffsetY = Parser.GetFloat(i, 15);
		E.MissileImpactOffsetZ = Parser.GetFloat(i, 16);
		E.AnimEventSoundID = Parser.GetUInt(i, 17);
		E.Flags = Parser.GetUInt(i, 18);
		E.MissileAttachment = Parser.GetUInt(i, 32);
		E.MissileFollowGroundHeight = Parser.GetFloat(i, 33);
		E.MissileFollowGroundDropSpeed = Parser.GetFloat(i, 34);

		IdIndex.Add(E.ID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogSpellVisualDbc, Log, TEXT("Loaded SpellVisual.dbc: %d records"), Entries.Num());
	return true;
}

const FSpellVisualDbcEntry* FSpellVisualDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}