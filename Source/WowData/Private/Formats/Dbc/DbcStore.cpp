#include "Formats/Dbc/DbcStore.h"
#include "Mpq/MpqManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDbcStore, Log, All);

FDbcStore& FDbcStore::Get()
{
    static FDbcStore Instance;
    return Instance;
}

static bool LoadSingleDbc(FMpqManager& Mpq, const TCHAR* DbcPath, FDbcParser& OutParser)
{
    TArray<uint8> Data;
    if (!Mpq.ReadFile(DbcPath, Data))
    {
        UE_LOG(LogDbcStore, Warning, TEXT("Failed to read %s from MPQ"), DbcPath);
        return false;
    }
    if (!OutParser.Parse(Data))
    {
        UE_LOG(LogDbcStore, Warning, TEXT("Failed to parse %s"), DbcPath);
        return false;
    }
    return true;
}

bool FDbcStore::LoadAll(FMpqManager& Mpq)
{
    FDbcParser Parser;
    int32 Loaded = 0;

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\Map.dbc"), Parser))
    {
        MapDbc.Load(Parser);
        ++Loaded;

        // Debug: print first 3 map entries
        for (int32 i = 0; i < FMath::Min(3, MapDbc.Num()); ++i)
        {
            const FMapDbcEntry& E = MapDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  Map[%d]: ID=%d Name='%s' Type=%d"), i, E.ID, *E.Name, E.MapType);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\AreaTable.dbc"), Parser))
    {
        AreaTableDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, AreaTableDbc.Num()); ++i)
        {
            const FAreaTableDbcEntry& E = AreaTableDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  Area[%d]: ID=%d MapID=%d Name='%s'"), i, E.ID, E.MapID, *E.Name);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\Light.dbc"), Parser))
    {
        LightDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, LightDbc.Num()); ++i)
        {
            const FLightDbcEntry& E = LightDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  Light[%d]: ID=%d MapID=%d Pos=(%.1f,%.1f,%.1f)"), i, E.ID, E.MapID, E.X, E.Y, E.Z);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\LightParams.dbc"), Parser))
    {
        LightParamsDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, LightParamsDbc.Num()); ++i)
        {
            const FLightParamsDbcEntry& E = LightParamsDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  LightParams[%d]: ID=%d SkyboxID=%d Glow=%.2f"), i, E.ID, E.SkyboxID, E.Glow);
        }
    }

    bLoaded = Loaded > 0;
    UE_LOG(LogDbcStore, Log, TEXT("DbcStore: loaded %d/4 DBC tables"), Loaded);
    return bLoaded;
}
