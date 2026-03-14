#include "Formats/Dbc/GroundEffectTextureDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogGroundEffectTextureDbc, Log, All);

bool FGroundEffectTextureDbc::Load(const FDbcParser& Parser)
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
		FGroundEffectTextureDbcEntry E;
		E.ID = Parser.GetUInt(i, 0);
		E.DoodadID[0] = Parser.GetUInt(i, 1);
		E.DoodadID[1] = Parser.GetUInt(i, 2);
		E.DoodadID[2] = Parser.GetUInt(i, 3);
		E.DoodadID[3] = Parser.GetUInt(i, 4);
		E.DoodadWeight[0] = static_cast<int32>(Parser.GetUInt(i, 5));
		E.DoodadWeight[1] = static_cast<int32>(Parser.GetUInt(i, 6));
		E.DoodadWeight[2] = static_cast<int32>(Parser.GetUInt(i, 7));
		E.DoodadWeight[3] = static_cast<int32>(Parser.GetUInt(i, 8));
		E.Density = Parser.GetUInt(i, 9);
		E.TerrainTypeID = Parser.GetUInt(i, 10);

		IdIndex.Add(E.ID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogGroundEffectTextureDbc, Log, TEXT("Loaded GroundEffectTexture.dbc: %d records"), Entries.Num());
	return true;
}

const FGroundEffectTextureDbcEntry* FGroundEffectTextureDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}