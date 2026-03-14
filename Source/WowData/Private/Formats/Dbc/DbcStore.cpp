#include "Formats/Dbc/DbcStore.h"
#include "Mpq/MpqManager.h"
#include <initializer_list>

DEFINE_LOG_CATEGORY_STATIC(LogDbcStore, Log, All);

FDbcStore& FDbcStore::Get()
{
    static FDbcStore Instance;
    return Instance;
}

static bool TryLoadSingleDbc(FMpqManager& Mpq, const TCHAR* DbcPath, FDbcParser& OutParser)
{
    TArray<uint8> Data;
    if (!Mpq.ReadFile(DbcPath, Data))
    {
        return false;
    }
    if (!OutParser.Parse(Data))
    {
        return false;
    }
    return true;
}

static bool LoadSingleDbc(FMpqManager& Mpq, const TCHAR* DbcPath, FDbcParser& OutParser)
{
    if (!TryLoadSingleDbc(Mpq, DbcPath, OutParser))
    {
        UE_LOG(LogDbcStore, Warning, TEXT("Failed to load %s"), DbcPath);
        return false;
    }

    return true;
}

static bool LoadSingleDbcAny(FMpqManager& Mpq, std::initializer_list<const TCHAR*> Paths, FDbcParser& OutParser, FString& OutLoadedPath)
{
    for (const TCHAR* Path : Paths)
    {
        if (TryLoadSingleDbc(Mpq, Path, OutParser))
        {
            OutLoadedPath = Path;
            return true;
        }
    }

    FString Attempted;
    bool bFirst = true;
    for (const TCHAR* Path : Paths)
    {
        if (!bFirst)
        {
            Attempted += TEXT(", ");
        }
        Attempted += Path;
        bFirst = false;
    }

    UE_LOG(LogDbcStore, Warning, TEXT("Failed to load any of: %s"), *Attempted);
    return false;
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

    FString LoadedPath;
    // WotLK stores these bands as LightIntBand.dbc; keep the wrapper name aligned with the project specs.
    if (LoadSingleDbcAny(Mpq, {TEXT("DBFilesClient\\LightIntParams.dbc"), TEXT("DBFilesClient\\LightIntBand.dbc")}, Parser, LoadedPath))
    {
        LightIntParamsDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, LightIntParamsDbc.Num()); ++i)
        {
            const FLightIntParamsDbcEntry& E = LightIntParamsDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  LightIntParams[%d]: ID=%d Entries=%d FirstTime=%d Source='%s'"),
                i, E.ID, E.EntryCount, E.Times[0], *LoadedPath);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\LiquidType.dbc"), Parser))
    {
        LiquidTypeDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, LiquidTypeDbc.Num()); ++i)
        {
            const FLiquidTypeDbcEntry& E = LiquidTypeDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  LiquidType[%d]: ID=%d Name='%s' Type=%d Spell=%d"),
                i, E.ID, *E.Name, E.Type, E.SpellID);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\AnimationData.dbc"), Parser))
    {
        AnimationDataDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, AnimationDataDbc.Num()); ++i)
        {
            const FAnimationDataDbcEntry& E = AnimationDataDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  AnimationData[%d]: ID=%d Name='%s' Flags=%d Fallback=%d"),
                i, E.ID, *E.Name, E.Flags, E.Fallback);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\ChrRaces.dbc"), Parser))
    {
        ChrRacesDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, ChrRacesDbc.Num()); ++i)
        {
            const FChrRacesDbcEntry& E = ChrRacesDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  ChrRaces[%d]: ID=%d Name='%s' Prefix='%s' Models=%d/%d"),
                i, E.ID, *E.Name, *E.ClientPrefix, E.MaleModelID, E.FemaleModelID);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\CharSections.dbc"), Parser))
    {
        CharSectionsDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, CharSectionsDbc.Num()); ++i)
        {
            const FCharSectionsDbcEntry& E = CharSectionsDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  CharSections[%d]: ID=%d Race=%d Sex=%d Type=%d Variation=%d Color=%d"),
                i, E.ID, E.RaceID, E.SexID, E.Type, E.Variation, E.Color);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\CreatureDisplayInfo.dbc"), Parser))
    {
        CreatureDisplayInfoDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, CreatureDisplayInfoDbc.Num()); ++i)
        {
            const FCreatureDisplayInfoDbcEntry& E = CreatureDisplayInfoDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  CreatureDisplayInfo[%d]: ID=%d Model=%d Scale=%.2f Tex1='%s'"),
                i, E.ID, E.ModelID, E.Scale, *E.Texture1);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\CreatureModelData.dbc"), Parser))
    {
        CreatureModelDataDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, CreatureModelDataDbc.Num()); ++i)
        {
            const FCreatureModelDataDbcEntry& E = CreatureModelDataDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  CreatureModelData[%d]: ID=%d Path='%s' SizeClass=%d Scale=%.2f"),
                i, E.ID, *E.ModelPath, E.SizeClass, E.Scale);
        }
    }

    if (LoadSingleDbc(Mpq, TEXT("DBFilesClient\\ItemDisplayInfo.dbc"), Parser))
    {
        ItemDisplayInfoDbc.Load(Parser);
        ++Loaded;

        for (int32 i = 0; i < FMath::Min(3, ItemDisplayInfoDbc.Num()); ++i)
        {
            const FItemDisplayInfoDbcEntry& E = ItemDisplayInfoDbc.GetAll()[i];
            UE_LOG(LogDbcStore, Log, TEXT("  ItemDisplayInfo[%d]: ID=%d Icon='%s' ModelL='%s' ModelR='%s'"),
                i, E.ID, *E.Icon1, *E.ModelNames[0], *E.ModelNames[1]);
        }
    }

    bLoaded = Loaded > 0;
    UE_LOG(LogDbcStore, Log, TEXT("DbcStore: loaded %d/12 DBC tables"), Loaded);
    return bLoaded;
}
